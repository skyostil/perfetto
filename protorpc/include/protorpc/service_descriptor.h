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

#ifndef PROTORPC_INCLUDE_PROTORPC_SERVICE_DESCRIPTOR_H_
#define PROTORPC_INCLUDE_PROTORPC_SERVICE_DESCRIPTOR_H_

#include <functional>
#include <string>
#include <utility>
#include <vector>

namespace perfetto {
namespace protorpc {

class Service;
class ServiceRequestBase;
class ServiceReplyBase;

class ServiceDescriptor {
 public:
  struct Method {
    using MethodPtr = void (Service::*)(ServiceRequestBase, ServiceReplyBase);
    std::string name;
    std::function<std::unique_ptr<ProtoMessage>(const std::string&)> decoder;
    std::function<std::unique_ptr<ProtoMessage>()> new_reply_obj;
    MethodPtr function = nullptr;
  };
  ServiceDescriptor() = default;
  ServiceDescriptor(ServiceDescriptor&&) noexcept = default;
  ServiceDescriptor& operator=(ServiceDescriptor&&) = default;

  Service* service = nullptr;
  std::string service_name;
  std::vector<Method> methods;

 private:
  ServiceDescriptor(const ServiceDescriptor&) = delete;
  ServiceDescriptor& operator=(const ServiceDescriptor&) = delete;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_DESCRIPTOR_H_
