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

#ifndef PROTORPC_SRC_HOST_IMPL_H_
#define PROTORPC_SRC_HOST_IMPL_H_

#include <map>
#include <set>
#include <vector>

#include "cpp_common/task_runner.h"
#include "protorpc/host.h"
#include "protorpc/src/unix_socket.h"

namespace perfetto {
namespace protorpc {

class HostImpl : public Host {
 public:
  HostImpl(const char* socket_name, TaskRunner*);
  ~HostImpl() override;

  // Host implementation.
  bool Start() override;
  ServiceID ExposeService(const ServiceDescriptor&) override;

  // reply == nullptr means abort.
  void SendReply(RequestID, std::unique_ptr<ProtoMessage> reply) override;

  void AddHandle(HostHandle*) override;
  void RemoveHandle(HostHandle*) override;

 private:
  struct ClientConnection {
    ~ClientConnection();
    UnixSocket sock;
    std::vector<char> rxbuf;
  };
  HostImpl(const HostImpl&) = delete;
  HostImpl& operator=(const HostImpl&) = delete;

  bool Initialize(const char* socket_name);
  void OnNewConnection();
  void OnDataAvailable(int client_fd);
  void OnClientDisconnect(int client_fd);

  const char* const socket_name_;
  TaskRunner* const task_runner_;
  std::set<HostHandle*> handles_;
  UnixSocket sock_;  // The listening socket.
  std::map<int, std::unique_ptr<ClientConnection>> clients_;
};

}  // namespace perfetto
}  // namespace protorpc

#endif  // PROTORPC_SRC_HOST_IMPL_H_
