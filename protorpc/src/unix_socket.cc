/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "protorpc/src/unix_socket.h"

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

#include <string.h>

#include <algorithm>
#include <memory>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/task_runner.h"
#include "base/utils.h"

namespace perfetto {
namespace protorpc {

namespace {

// MSG_NOSIGNAL is not supported on Mac OS X, but in that case the socket is
// created with SO_NOSIGPIPE (See InitializeSocket()).
#if BUILDFLAG(OS_MACOSX)
constexpr int kNoSigPipe = 0;
#else
constexpr int kNoSigPipe = MSG_NOSIGNAL;
#endif

// Android takes an int instead of socklen_t for the control buffer size.
#if BUILDFLAG(OS_ANDROID)
using cbuf_len_t = size_t;
#else
using cbuf_len_t = socklen_t;
#endif

bool MakeSockAddr(const char* socket_name,
                  sockaddr_un* addr,
                  socklen_t* addr_size) {
  memset(addr, 0, sizeof(sockaddr_un));
  const size_t name_len = strlen(socket_name);
  if (name_len >= sizeof(addr->sun_path))
    return false;
  memcpy(addr->sun_path, socket_name, name_len);
  if (addr->sun_path[0] == '@')
    addr->sun_path[0] = '\0';
  addr->sun_family = AF_UNIX;
  *addr_size = static_cast<socklen_t>(
      __builtin_offsetof(sockaddr_un, sun_path) + name_len + 1);
  return true;
}

}  // namespace

UnixSocket::UnixSocket(EventListener* event_listener,
                       base::TaskRunner* task_runner)
    : event_listener_(event_listener),
      task_runner_(task_runner),
      weak_ref_(new WeakRef(this)) {}

UnixSocket::~UnixSocket() {
  weak_ref_->sock = nullptr;  // This will no-op any future callback.
  Shutdown();
}

bool UnixSocket::InitializeSocket() {
  PERFETTO_DCHECK(state_ == State::kNotInitialized);
  if (!fd_)
    fd_.reset(socket(AF_UNIX, SOCK_STREAM, 0));
  if (!fd_)
    return false;
#if BUILDFLAG(OS_MACOSX)
  const int no_sigpipe = 1;
  setsockopt(*fd_, SOL_SOCKET, SO_NOSIGPIPE, &no_sigpipe, sizeof(no_sigpipe));
#endif
  // There is no reason why a socket should outlive the process in case of
  // exec() by default, this is just working around a broken unix design.
  int fcntl_res = fcntl(*fd_, FD_CLOEXEC);
  PERFETTO_DCHECK(fcntl_res == 0);

  // Set non-blocking mode.
  int flags = fcntl(*fd_, F_GETFL, 0);
  flags |= O_NONBLOCK;
  fcntl_res = fcntl(fd(), F_SETFL, flags);
  PERFETTO_CHECK(fcntl_res == 0);

  std::shared_ptr<WeakRef> weak_ref = weak_ref_;
  task_runner_->AddFileDescriptorWatch(*fd_, [weak_ref]() {
    if (weak_ref->sock)
      weak_ref->sock->OnEvent();
  });
  state_ = State::kDisconnected;
  return true;
}

bool UnixSocket::Listen(const char* socket_name) {
  if (!InitializeSocket())
    return false;

  sockaddr_un addr;
  socklen_t addr_size;
  if (!MakeSockAddr(socket_name, &addr, &addr_size))
    return false;

// Android takes an int as 3rd argument of bind() instead of socklen_t.
#if BUILDFLAG(OS_ANDROID)
  const int bind_size = static_cast<int>(addr_size);
#else
  const socklen_t bind_size = addr_size;
#endif

  if (bind(*fd_, reinterpret_cast<sockaddr*>(&addr), bind_size)) {
    PERFETTO_DPLOG("bind()");
    return false;
  }
  if (listen(*fd_, SOMAXCONN)) {
    PERFETTO_DPLOG("listen()");
    return false;
  }

  state_ = State::kListening;
  return true;
}

bool UnixSocket::Connect(const char* socket_name) {
  if (state_ == State::kNotInitialized) {
    if (!InitializeSocket())
      return false;
  }

  sockaddr_un addr;
  socklen_t addr_size;
  if (!MakeSockAddr(socket_name, &addr, &addr_size))
    return false;

  int res = PERFETTO_EINTR(
      connect(*fd_, reinterpret_cast<sockaddr*>(&addr), addr_size));
  if (res && errno != EINPROGRESS)
    return false;

  // Would be quite unusual for a non-blocking socket to connect() straight away
  // rather than returning EINPROGRESS, but won't be surprised if some kernel
  // short circuits that for UNIX sockets. In this case just trigger an OnEvent
  // without waiting for the FD watch.
  if (res == 0) {
    std::shared_ptr<WeakRef> weak_ref = weak_ref_;
    task_runner_->PostTask([weak_ref]() {
      if (weak_ref->sock)
        weak_ref->sock->OnEvent();
    });
  }

  state_ = State::kConnecting;
  return true;
}

void UnixSocket::OnEvent() {
  // This would be weird because in this state we don't have setup the fd
  // watch yet.
  PERFETTO_DCHECK(state_ != State::kNotInitialized);

  if (state_ == State::kDisconnected)
    return;  // Some Spurious event.

  if (state_ == State::kConnected)
    return event_listener_->OnDataAvailable(this);

  if (state_ == State::kConnecting) {
    PERFETTO_DCHECK(fd_);
    int sock_err = EINVAL;
    socklen_t err_len = sizeof(sock_err);
    int res = getsockopt(*fd_, SOL_SOCKET, SO_ERROR, &sock_err, &err_len);
    if (res == 0 && sock_err == EINPROGRESS)
      return;  // Not connected yet, just a spurious FD watch wakeup.
    if (res == 0 && sock_err == 0) {
      state_ = State::kConnected;
      event_listener_->OnConnect(this, true /* connected */);
      return;
    }
    errno = sock_err;
    return event_listener_->OnConnect(this, false /* connected */);
  }

  // New incoming connection.
  if (state_ == State::kListening) {
    // There could be more than one incoming connection behind each FD watch
    // notification. Drain'em all.
    for (;;) {
      sockaddr_un cli_addr = {};
      socklen_t size = sizeof(cli_addr);
      base::ScopedFile new_fd(PERFETTO_EINTR(
          accept(*fd_, reinterpret_cast<sockaddr*>(&cli_addr), &size)));
      if (!new_fd)
        return;
      std::unique_ptr<UnixSocket> new_sock(
          new UnixSocket(event_listener_, task_runner_));
      new_sock->fd_ = std::move(new_fd);
      bool initialized = new_sock->InitializeSocket();
      PERFETTO_CHECK(initialized);  // This can't fail.
      new_sock->state_ = State::kConnected;
      event_listener_->OnNewIncomingConnection(this, std::move(new_sock));
    }
  }
}

bool UnixSocket::Send(const std::string& msg) {
  return Send(msg.c_str(), msg.size() + 1);
}

bool UnixSocket::Send(const void* msg, size_t len, int wired_fd) {
  if (state_ != State::kConnected)
    return false;

  msghdr msg_hdr = {};
  iovec iov = {const_cast<void*>(msg), len};
  msg_hdr.msg_iov = &iov;
  msg_hdr.msg_iovlen = 1;
  alignas(cmsghdr) char control_buf[256];

  if (wired_fd > -1) {
    const cbuf_len_t control_buf_len =
        static_cast<cbuf_len_t>(CMSG_SPACE(sizeof(int)));
    PERFETTO_CHECK(control_buf_len <= sizeof(control_buf));
    memset(control_buf, 0, sizeof(control_buf));
    msg_hdr.msg_control = control_buf;
    msg_hdr.msg_controllen = control_buf_len;
    struct cmsghdr* cmsg = CMSG_FIRSTHDR(&msg_hdr);
    cmsg->cmsg_level = SOL_SOCKET;
    cmsg->cmsg_type = SCM_RIGHTS;
    cmsg->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(cmsg), &wired_fd, sizeof(int));
    msg_hdr.msg_controllen = cmsg->cmsg_len;
  }

  const ssize_t sz = PERFETTO_EINTR(sendmsg(*fd_, &msg_hdr, kNoSigPipe));
  if (sz > 0) {
    // There should be no way a non-blocking socket returns < |len|.
    // If the queueing fails, sendmsg() must return -1 + errno = EWOULDBLOCK.
    PERFETTO_CHECK(static_cast<size_t>(sz) == len);
    return true;
  }
  if (sz == 0) {
    PERFETTO_DCHECK(len == 0);
    return true;
  }
  if (errno == EAGAIN || errno == EWOULDBLOCK) {
    // A genuine out-of-buffer error. The client should retry or give up.
    return false;
  }
  // Either the the other endpoint disconnect (ECONNRESET) or some other error
  // happened.
  PERFETTO_DPLOG("sendmsg() failed");
  Shutdown();
  return false;
}

void UnixSocket::Shutdown() {
  if (state_ == State::kConnected) {
    std::shared_ptr<WeakRef>& weak_ref = weak_ref_;
    task_runner_->PostTask([weak_ref]() {
      if (weak_ref->sock)
        weak_ref->sock->event_listener_->OnDisconnect(weak_ref->sock);
    });
  }
  if (fd_) {
    shutdown(*fd_, SHUT_RDWR);
    task_runner_->RemoveFileDescriptorWatch(*fd_);
    fd_.reset();
  }
  state_ = State::kNotInitialized;
}

size_t UnixSocket::Recv(void* msg, size_t len, base::ScopedFile* wired_fd) {
  if (state_ != State::kConnected)
    return 0;

  msghdr msg_hdr = {};
  iovec iov = {msg, len};
  msg_hdr.msg_iov = &iov;
  msg_hdr.msg_iovlen = 1;
  alignas(cmsghdr) char control_buf[256];

  if (wired_fd) {
    msg_hdr.msg_control = control_buf;
    msg_hdr.msg_controllen = static_cast<cbuf_len_t>(CMSG_SPACE(sizeof(int)));
    PERFETTO_CHECK(msg_hdr.msg_controllen <= sizeof(control_buf));
  }
  const ssize_t sz = PERFETTO_EINTR(recvmsg(*fd_, &msg_hdr, kNoSigPipe));
  if (sz < 0 && (errno == EAGAIN || errno == EWOULDBLOCK))
    return 0;
  if (sz == 0) {
    Shutdown();
    return 0;
  }
  PERFETTO_CHECK(static_cast<size_t>(sz) <= len);

  int* wire_fds = nullptr;
  uint32_t wire_fds_len = 0;

  if (msg_hdr.msg_controllen > 0) {
    for (cmsghdr* cmsg = CMSG_FIRSTHDR(&msg_hdr); cmsg;
         cmsg = CMSG_NXTHDR(&msg_hdr, cmsg)) {
      const size_t payload_len = cmsg->cmsg_len - CMSG_LEN(0);
      if (cmsg->cmsg_level == SOL_SOCKET && cmsg->cmsg_type == SCM_RIGHTS) {
        PERFETTO_DCHECK(payload_len % sizeof(int) == 0u);
        PERFETTO_DCHECK(wire_fds == nullptr);
        wire_fds = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        wire_fds_len = static_cast<uint32_t>(payload_len / sizeof(int));
      }
    }
  }

  if (msg_hdr.msg_flags & MSG_TRUNC || msg_hdr.msg_flags & MSG_CTRUNC) {
    for (size_t i = 0; i < wire_fds_len; ++i)
      close(wire_fds[i]);
    errno = EMSGSIZE;
    Shutdown();
    return 0;
  }

  for (size_t i = 0; wire_fds && i < wire_fds_len; ++i) {
    if (wired_fd && i == 0) {
      wired_fd->reset(wire_fds[i]);
    } else {
      close(wire_fds[i]);
    }
  }

  return static_cast<size_t>(sz);
}

std::string UnixSocket::RecvString(size_t max_length) {
  std::unique_ptr<char[]> buf(new char[max_length + 1]);
  size_t rsize = Recv(buf.get(), max_length);
  PERFETTO_CHECK(static_cast<size_t>(rsize) <= max_length);
  buf[static_cast<size_t>(rsize)] = '\0';
  return std::string(buf.get());
}

UnixSocket::EventListener::~EventListener() {}
void UnixSocket::EventListener::OnNewIncomingConnection(
    UnixSocket*,
    std::unique_ptr<UnixSocket>) {}
void UnixSocket::EventListener::OnConnect(UnixSocket*, bool) {}
void UnixSocket::EventListener::OnDisconnect(UnixSocket*) {}
void UnixSocket::EventListener::OnDataAvailable(UnixSocket*) {}

}  // namespace protorpc
}  // namespace perfetto
