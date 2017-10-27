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
#include <string>
#include <vector>

#include "cpp_common/task_runner.h"
#include "protorpc/deferred.h"
#include "protorpc/host.h"
#include "protorpc/src/rpc_frame_decoder.h"
#include "protorpc/src/unix_socket.h"

namespace perfetto {
namespace protorpc {

class RPCFrame;

class HostImpl : public Host {
 public:
  HostImpl(const char* socket_name, TaskRunner*);
  ~HostImpl() override;

  void set_weak_ptr(const std::weak_ptr<HostImpl>& ptr) { weak_ptr_ = ptr; }

  // Host implementation.
  bool Start() override;
  bool ExposeService(const std::shared_ptr<Service>&) override;

 private:
  struct ClientConnection {
    ~ClientConnection();
    ClientID id;
    UnixSocket sock;
    RPCFrameDecoder frame_decoder;
  };
  struct ExposedService {
    std::shared_ptr<Service> instance;
    ServiceID id;
    std::string name;
  };
  HostImpl(const HostImpl&) = delete;
  HostImpl& operator=(const HostImpl&) = delete;

  bool Initialize(const char* socket_name);
  void OnNewConnection();
  void OnDataAvailable(ClientID);
  void OnClientDisconnect(ClientID);
  void OnReceivedRPCFrame(ClientID, ClientConnection*, const RPCFrame&);
  void OnBindService(ClientConnection*, const RPCFrame&);
  void OnInvokeMethod(ClientConnection*, const RPCFrame&);
  void ReplyToMethodInvocation(ClientID,
                               RequestID,
                               Deferred<ProtoMessage>);

  const ExposedService* GetServiceByName(const std::string&);
  void SendRPCFrame(ClientConnection*, const RPCFrame&);

  std::weak_ptr<HostImpl> weak_ptr_;
  const char* const socket_name_;
  TaskRunner* const task_runner_;
  std::map<ServiceID, ExposedService> services_;
  UnixSocket sock_;  // The listening socket.
  std::map<ClientID, std::unique_ptr<ClientConnection>> clients_;
  ServiceID last_service_id_ = 0;
  ClientID last_client_id_ = 0;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_HOST_IMPL_H_
