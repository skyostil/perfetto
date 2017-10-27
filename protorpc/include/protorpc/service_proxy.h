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

#ifndef PROTORPC_INCLUDE_PROTORPC_SERVICE_PROXY_H_
#define PROTORPC_INCLUDE_PROTORPC_SERVICE_PROXY_H_

#include "protorpc/basic_types.h"

#include <functional>
#include <map>
#include <memory>
#include <string>

#include "protorpc/deferred.h"

namespace perfetto {
namespace protorpc {

class Client;
class ServiceDescriptor;

class ServiceProxy {
 public:
  ServiceProxy();
  virtual ~ServiceProxy();

  virtual void OnConnect();
  virtual void OnConnectionFailed();

  bool connected() const { return service_id_ != 0; }

  void InitializeBinding(const std::weak_ptr<ServiceProxy>&,
                         const std::weak_ptr<Client>&,
                         ServiceID,
                         std::map<std::string, MethodID>);

  // Called by the autogenerated ServiceProxy subclasses.
  template <typename T>
  void BeginInvoke(const std::string& method_name,
                   const ProtoMessage& request,
                   Deferred<T> reply) {
    BeginInvokeGeneric(method_name, request, reply.template As<ProtoMessage>());
  }

  void BeginInvokeGeneric(const std::string& method_name,
                          const ProtoMessage& request,
                          Deferred<ProtoMessage> reply);

  // Called by ClientImpl.
  // reply_args == nullptr means request failure.
  void EndInvoke(RequestID,
                 std::unique_ptr<ProtoMessage> reply_arg,
                 bool has_more);

  // implemented by the autogenerated class.
  virtual const ServiceDescriptor& GetDescriptor() = 0;

 private:
  // This is essentially a weak ptr factory, as weak_ptr(s) are copyable.
  std::weak_ptr<ServiceProxy> weak_ptr_self_;
  std::weak_ptr<Client> client_;
  ServiceID service_id_ = 0;
  std::map<std::string, MethodID> remote_method_ids_;
  std::map<RequestID, Deferred<ProtoMessage>> pending_callbacks_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_PROXY_H_
