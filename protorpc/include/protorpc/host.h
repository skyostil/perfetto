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

#ifndef PROTORPC_INCLUDE_PROTORPC_HOST_H_
#define PROTORPC_INCLUDE_PROTORPC_HOST_H_

#include <memory>

#include "protorpc/basic_types.h"

namespace perfetto {

class TaskRunner;

namespace protorpc {

class HostHandle;
class ServiceDescriptor;

class Host {
 public:
  static std::unique_ptr<Host> CreateInstance(const char* socket_name,
                                              TaskRunner*);
  virtual ~Host() = default;

  virtual bool Start() = 0;

  // Once exposed the service cannot be unexposed. The Service instance is
  // expect to stay alive until the Host is alive.
  virtual ServiceID ExposeService(ServiceDescriptor) = 0;

  virtual void ReplyToMethodInvocation(ClientID,
                                       RequestID,
                                       std::unique_ptr<ProtoMessage>) = 0;

  virtual void AddHandle(HostHandle*) = 0;
  virtual void RemoveHandle(HostHandle*) = 0;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_HOST_H_
