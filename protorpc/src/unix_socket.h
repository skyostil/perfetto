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

#include <errno.h>
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
// transfer file descriptors over the network. None of the methods in this class
// are blocking.
// The main design goal is API simplicity and strong guarantees on the
// EventListener callbacks, in order to avoid ending in some undefined state.
// In case of any error it will aggressively just shut down the socket and
// notify the failure with OnConnect(false) or OnDisconnect() depending on the
// state of the socket (see below).
// EventListener callbacks stop happening as soon as the instance is destroyed.
//
// Lifecycle of a client socket:
//
//                           Connect()
//                               |
//            +------------------+------------------+
//            | (success)                           | (failure or Shutdown())
//            V                                     V
//     OnConnect(true)                         OnConnect(false)
//            |
//            V
//    OnDataAvailable()
//            |
//            V
//     OnDisconnect()  (failure or shutdown)
//
//
// Lifecycle of a server socket:
//
//                          Listen()  --> returns false in case of errors.
//                             |
//                             V
//              OnNewIncomingConnection(new_socket)
//
//          (|new_socket| inherits the same EventListener)
//                             |
//                             V
//                     OnDataAvailable()
//                             | (failure or Shutdown())
//                             V
//                       OnDisconnect()
class UnixSocket {
 public:
  class EventListener {
   public:
    virtual ~EventListener();

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
    kNotInitialized = 0,
    kDisconnected,
    kConnecting,
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
  bool Listen(const std::string& socket_name);

  // Creates a Unix domain socket and connects to the listening endpoint.
  // EventListener::OnConnect(bool success) will be called, whether the Connect
  // succeeded or not.
  void Connect(const std::string& socket_name);

  // Shutdowns the current connection, if any. If the socket was Listen()-ing,
  // stops listening. The socket goes back to kNotInitialized state, so it can
  // be reused with Listen() or Connect().
  void Shutdown();

  // Returns true is the message was queued, false if there was no space in the
  // output buffer, in which case the client should retry or give up.
  // If any other error happens the socket will be shutdown and
  // EventListener::OnDisconnect() will be called.
  // If the socket is not connected, Send() will just return false.
  // Does not append a null string terminator to msg in any case.
  bool Send(const void* msg, size_t len, int wired_fd = -1);
  bool Send(const std::string& msg);

  // Returns the number of bytes (<= |len|) written in |msg| or 0 if there
  // is no data in the buffer to read or an error occurs (in which case a
  // EventListener::OnDisconnect() will follow).
  // Returns 0 if the socet is not connected.
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
  errno_t last_error() const { return last_error_; }

 private:
  // Used to decouple the lifetime of the UnixSocket from the callbacks
  // registered on the TaskRunner, which might happen after UnixSocket has been
  // destroyed. This is essentially a single-instance weak_ptr<UnixSocket>.
  // Unfortunately C++11's weak_ptr would require UnixSocket to be a shared_ptr,
  // which is undesirable here. The |sock| pointer is invalidated by the dtor
  // of UnixSocket.
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
  void OnEvent();
  void NotifyConnectionState(bool success);

  base::ScopedFile fd_;
  State state_ = State::kNotInitialized;
  errno_t last_error_ = 0;
  EventListener* event_listener_;
  base::TaskRunner* task_runner_;
  std::shared_ptr<WeakRef> weak_ref_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_UNIX_SOCKET_H_
