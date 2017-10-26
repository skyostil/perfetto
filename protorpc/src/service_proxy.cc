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

#include "protorpc/service_proxy.h"

#include <utility>

#include "cpp_common/base.h"
#include "google/protobuf/message_lite.h"
#include "protorpc/client.h"
#include "protorpc/method_invocation_reply.h"
#include "protorpc/service_descriptor.h"

namespace perfetto {
namespace protorpc {

ServiceProxy::ServiceProxy(std::weak_ptr<ServiceProxy> weak_ptr_self,
                           std::weak_ptr<Client> client,
                           ServiceID service_id,
                           std::map<std::string, MethodID> remote_method_ids)
    : weak_ptr_self_(weak_ptr_self),
      client_(client),
      service_id_(service_id),
      remote_method_ids_(std::move(remote_method_ids)) {}

ServiceProxy::~ServiceProxy() = default;

void ServiceProxy::BeginInvokeGeneric(
    const std::string& method_name,
    ProtoMessage* method_args,
    std::function<void(MethodInvocationReply<ProtoMessage>)> callback) {
  auto remote_method_it = remote_method_ids_.find(method_name);
  std::shared_ptr<Client> client = client_.lock();
  RequestID request_id = 0;
  if (remote_method_it != remote_method_ids_.end() && client)
    request_id = client->BeginInvoke(service_id_, remote_method_it->second,
                                     method_args, weak_ptr_self_);
  if (!request_id) {
    callback(MethodInvocationReply<ProtoMessage>(nullptr, true /*eof*/));
    return;
  }
  DCHECK(pending_callbacks_.count(request_id) == 0);
  pending_callbacks_.emplace(request_id, std::move(callback));
}

void ServiceProxy::EndInvoke(RequestID request_id,
                             std::unique_ptr<ProtoMessage> result,
                             bool eof) {
  auto callback_it = pending_callbacks_.find(request_id);
  if (callback_it == pending_callbacks_.end()) {
    DCHECK(false);
    return;
  }
  MethodInvocationReply<ProtoMessage> callback_result(std::move(result),
  eof); auto callback = std::move(callback_it->second);
  pending_callbacks_.erase(callback_it);
  callback(std::move(callback_result));
}

}  // namespace protorpc
}  // namespace perfetto
