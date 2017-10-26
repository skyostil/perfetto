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

#ifndef PROTORPC_INCLUDE_PROTORPC_CLIENT_H_
#define PROTORPC_INCLUDE_PROTORPC_CLIENT_H_

#include <functional>
#include <memory>

#include "protorpc/basic_types.h"

namespace perfetto {

class TaskRunner;

namespace protorpc {
class ServiceProxy;

class Client {
 public:
  static std::shared_ptr<Client> CreateInstance(const char* socket_name,
                                                TaskRunner*);
  virtual ~Client() = default;

  virtual void BindService(const std::weak_ptr<ServiceProxy>&) = 0;

  virtual RequestID BeginInvoke(ServiceID,
                                const std::string& method_name,
                                MethodID remote_method_id,
                                ProtoMessage*,
                                const std::weak_ptr<ServiceProxy>&) = 0;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_CLIENT_H_
