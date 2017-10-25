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

#include "protorpc/src/host_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <utility>

#include "cpp_common/task_runner.h"
#include "protorpc/host_handle.h"
#include "wire_protocol.pb.h"

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define BYTE_SWAP_TO_LE32(x) (x)
#else
#error Unimplemented for big endian archs.
#endif

namespace perfetto {
namespace protorpc {

// static
std::unique_ptr<Host> Host::CreateInstance(const char* socket_name,
                                           TaskRunner* task_runner) {
  return std::unique_ptr<Host>(new HostImpl(socket_name, task_runner));
}

HostImpl::HostImpl(const char* socket_name, TaskRunner* task_runner)
    : socket_name_(socket_name), task_runner_(task_runner) {}

HostImpl::~HostImpl() {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
  for (HostHandle* handle : handles_)
    handle->clear_host();
  task_runner_->RemoveFileDescriptorWatch(sock_.fd());
}

bool HostImpl::Start() {
  if (sock_.Listen(socket_name_))
    return false;
  sock_.SetBlockingIOMode(false);
  task_runner_->AddFileDescriptorWatch(
      sock_.fd(), std::bind(&HostImpl::OnNewConnection, this));
  return true;
}

void HostImpl::OnNewConnection() {
  UnixSocket cli_sock;
  while (sock_.Accept(&cli_sock)) {
    int cli_sock_fd = cli_sock.fd();
    if (cli_sock_fd < 0) {
      DCHECK(false);
      continue;
    }
    // TODO(primiano): careful with Send() and non-blocking mode.
    cli_sock.SetBlockingIOMode(false);
    std::unique_ptr<ClientConnection> client(new ClientConnection());
    client->sock = std::move(cli_sock);
    clients_[cli_sock_fd] = std::move(client);
    task_runner_->AddFileDescriptorWatch(
        cli_sock_fd, std::bind(&HostImpl::OnDataAvailable, this, cli_sock_fd));
  }
}

void HostImpl::OnDataAvailable(int client_fd) {
  auto client_it = clients_.find(client_fd);
  if (client_it == clients_.end()) {
    DCHECK(false);
    return;
  }
  ClientConnection& client = *client_it->second.get();

  // TODO(primiano): this implementation is terribly inefficient as keeps
  //  reallocating all the time to expand and shrink the buffer.
  std::vector<char>& rxbuf = client.rxbuf;

  ssize_t rsize;
  do {
    static const size_t kReadSize = 4096;
    const size_t prev_size = rxbuf.size();
    rxbuf.resize(prev_size + kReadSize);
    rsize = client.sock.Recv(rxbuf.data() + prev_size, kReadSize);
    rxbuf.resize(prev_size + std::max(0, static_cast<int>(rsize)));
    // TODO(primiano): Recv should return some different code to distinguish
    // EWOULDBLOCK from a generic error.
  } while (rsize > 0);

  while (rxbuf.size() > sizeof(uint32_t)) {
    uint32_t frame_size = 0;
    memcpy(&frame_size, rxbuf.data(), sizeof(uint32_t));
    frame_size = BYTE_SWAP_TO_LE32(frame_size);
    if (rxbuf.size() < frame_size + sizeof(uint32_t))
      break;
    RPCFrame frame;
    bool res = frame.ParseFromArray(rxbuf.data(), static_cast<int>(frame_size));
    if (res) {

    } else {
      DLOG("Received malformed frame. client:%d, size: %" PRIu32, client_fd,
           frame_size);
    }
    rxbuf.erase(rxbuf.begin(), rxbuf.begin() + frame_size + sizeof(uint32_t));
  }

  if (rsize == 0)
    return OnClientDisconnect(client_fd);
}

void HostImpl::OnClientDisconnect(int client_fd) {
  DCHECK(clients_.count(client_fd));
  DLOG("[protorpc::HostImpl] Client %d disconnected", client_fd);
  task_runner_->RemoveFileDescriptorWatch(client_fd);
  clients_.erase(client_fd);
}

ServiceID HostImpl::ExposeService(const ServiceDescriptor&) {
  return 0;
}

void HostImpl::SendReply(RequestID, std::unique_ptr<ProtoMessage> reply) {}

void HostImpl::AddHandle(HostHandle* handle) {
  handles_.insert(handle);
}

void HostImpl::RemoveHandle(HostHandle* handle) {
  handles_.erase(handle);
}

HostImpl::ClientConnection::~ClientConnection() = default;

}  // namespace protorpc
}  // namespace perfetto
