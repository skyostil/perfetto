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
#include "protorpc/service_descriptor.h"

namespace perfetto {
namespace protorpc {

namespace {
// A default implementation just to avoid having to null-check the event
// listener all the times.
class NoOpEventListener : public ServiceProxy::EventListener {
  void OnConnect() override {}
  void OnConnectionFailed() override {}
};
}  // namespace

ServiceProxy::ServiceProxy() : event_listener_(new NoOpEventListener()) {}
ServiceProxy::~ServiceProxy() = default;

void ServiceProxy::InitializeBinding(
    const std::weak_ptr<ServiceProxy>& weak_ptr_self,
    const std::weak_ptr<Client>& client,
    ServiceID service_id,
    std::map<std::string, MethodID> remote_method_ids) {
  weak_ptr_self_ = std::move(weak_ptr_self);
  client_ = client;
  service_id_ = service_id;
  remote_method_ids_ = std::move(remote_method_ids);
}

void ServiceProxy::BeginInvokeGeneric(const std::string& method_name,
                                      const ProtoMessage& request,
                                      Deferred<ProtoMessage> reply) {
  // |reply| will auto-resolve if it gets out of scope early.
  if (!connected()) {
    DCHECK(false);
    return;
  }
  auto remote_method_it = remote_method_ids_.find(method_name);
  std::shared_ptr<Client> client = client_.lock();
  RequestID request_id = 0;
  if (client && remote_method_it != remote_method_ids_.end())
    request_id =
        client->BeginInvoke(service_id_, method_name, remote_method_it->second,
                            request, weak_ptr_self_);
  if (!request_id)
    return;
  DLOG("BeginInvoke %llu", request_id);
  DCHECK(pending_callbacks_.count(request_id) == 0);
  pending_callbacks_.emplace(request_id, std::move(reply));
}

void ServiceProxy::EndInvoke(RequestID request_id,
                             std::unique_ptr<ProtoMessage> result,
                             bool has_more) {
  DLOG("EndInvoke %llu", request_id);
  auto callback_it = pending_callbacks_.find(request_id);
  if (callback_it == pending_callbacks_.end()) {
    DCHECK(false);
    return;
  }
  Deferred<ProtoMessage> reply(std::move(callback_it->second));
  reply.set_msg(std::move(result));
  reply.set_has_more(has_more);
  // TODO how do we handle streaming responses (when has_more == true)? Who
  // rebinds the callback? Maybe we should keep it around?
  pending_callbacks_.erase(callback_it);
  return reply.Resolve();
}

}  // namespace protorpc
}  // namespace perfetto
