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

#ifndef PROTORPC_SRC_CLIENT_IMPL_H_
#define PROTORPC_SRC_CLIENT_IMPL_H_

#include "cpp_common/task_runner.h"
#include "protorpc/client.h"
#include "protorpc/src/unix_socket.h"

#include <map>

namespace perfetto {
namespace protorpc {

class RPCFrame;

class ClientImpl : public Client {
 public:
  ClientImpl(const char* socket_name, TaskRunner*);
  ~ClientImpl() override;

  bool Connect();

  // Client implementation.
  void BindService(const std::string& service_name, BindServiceCallback) override;

 private:
  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  const char* const socket_name_;
  TaskRunner* const task_runner_;
  UnixSocket sock_;
  RequestID last_request_id_ = 0;
  std::map<RequestID, > queued_request_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_CLIENT_IMPL_H_
