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

#ifndef PROTORPC_SRC_UNIX_SOCKET_H_
#define PROTORPC_SRC_UNIX_SOCKET_H_

#include <stdint.h>
#include <sys/types.h>

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/scoped_file.h"

namespace perfetto {

namespace base {
class TaskRunner;
}  // namespace base.

namespace protorpc {

// A non-blocking UNIX domain socket in SOCK_STREAM mode. Allows also to
// transfer file descriptors over the network.
// None of the methods in this class are blocking.
// The main design goal is API simplicity and strong guarantees on the
// EventListener callbacks, in order to avoid ending in any undetermined state.
// It assumes that the client doesn't want to do any fancy error handling and
// in case of any error it will aggressively just shut down the socket and
// notify an OnDisconnect().
class UnixSocket {
 public:
  class EventListener {
   public:
    // After Listen().
    virtual void OnNewIncomingConnection(
        UnixSocket* self,
        std::unique_ptr<UnixSocket> new_connection);

    // After Connect(), whether successful or not.
    virtual void OnConnect(UnixSocket* self, bool connected);

    // After a sucessful Connect() or OnNewConnection(). Either the other
    // endpoint did disconnect or some other error happened.
    virtual void OnDisconnect(UnixSocket* self);

    // Whenever there is data available to Recv().
    virtual void OnDataAvailable(UnixSocket* self);
  };

  enum class State {
    kDisconnected = 0,
    kConnected,
    kListening  // Only for service sockets, after Listen()
  };

  // Guarantees that no event is called on the EventListener after the object
  // has been destroyed. Any queued callback will be dropped.
  UnixSocket(EventListener*, base::TaskRunner*);
  ~UnixSocket();

  // Creates a Unix domain socket and starts listening. If |socket_name|
  // starts with a '@', an abstract socket will be created (Linux/Android only).
  // Returns false in on failure (e.g., another socket with the same name is
  // already listening). New connections will be notified through
  // EventListener::OnNewConnection().
  bool Listen(const char* socket_name);

  // Creates a Unix domain socket and connects to the listening endpoint.
  // Return value:
  // false: an error occurred, no EventListener::OnConnect() will happen.
  // true: EventListener::OnConnect() will update on the outcome of the
  //       connection, either successful or not.
  bool Connect(const char* socket_name);

  // Returns true is the message was queued, false if there was no space in the
  // output buffer, in which case the client should retry or give up.
  // If any other error happens the socket will be shutdown and
  // EventListener::OnDisconnect() will be called.
  // If the socket is not connected, Send() will DCHECK in debug builds, and
  // return false in release.
  // Does not append a null string terminator to msg in any case.
  bool Send(const void* msg, size_t len, int wired_fd = -1);
  bool Send(const std::string& msg);

  // Returns the number of bytes (<= |len|) written in |msg| or 0 if there
  // is no data in the buffer to read or an error occurs (in which case a
  // EventListener::OnDisconnect() will follow).
  // DCHECK(s) in debug builds if the socet is not connected.
  // If the ScopedFile pointer is not null and a file descriptor is received, it
  // moves the received fd into that.
  size_t Recv(void* msg, size_t len, base::ScopedFile* = nullptr);

  // Only for tests. This is slower than Recv() as it requires a heap allocation
  // and a copy for the std::string. Guarantees that the returned string is null
  // terminated even if the underlying message sent by the peer is not.
  std::string RecvString(size_t max_length = 1024);

  EventListener* event_listener() const { return event_listener_; }
  bool is_connected() const { return state_ == State::kConnected; }
  bool is_listening() const { return state_ == State::kListening; }
  int fd() const { return fd_.get(); }

 private:
  // Used to decouple the lifetime of the UnixSocket from the callbacks
  // registered on the TaskRunner, without having to make the full UnixSocket
  // a shared_ptr (C++11 weak_ptr require that the object is a shared_ptr).
  // A shared_ptr<WeakRef> is passed around to the callbacks posted on the
  // task_runner_. The |sock| pointer is invalidated by the dtor.
  struct WeakRef {
    explicit WeakRef(UnixSocket* s) : sock(s) {}
    ~WeakRef() { sock = nullptr; }
    WeakRef(const WeakRef&) = delete;
    WeakRef& operator=(const WeakRef&) = delete;

    UnixSocket* sock;
  };

  UnixSocket(const UnixSocket&) = delete;
  UnixSocket& operator=(const UnixSocket&) = delete;

  bool InitializeSocket();
  void OnIncomingConnectionsAvailable();
  void OnEvent();
  void ShutdownAndNotifyEventListenerIfConnected();

  base::ScopedFile fd_;
  State state_ = State::kDisconnected;
  EventListener* event_listener_;
  TaskRunner* task_runner_;
  std::shared_ptr<WeakRef> weak_ref_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_UNIX_SOCKET_H_
