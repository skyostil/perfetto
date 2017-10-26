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

#include "protorpc/service_reply.h"

#include <utility>

#include "cpp_common/base.h"
#include "google/protobuf/message_lite.h"

namespace perfetto {
namespace protorpc {

ServiceReplyBase::ServiceReplyBase(ClientID client_id,
                                   RequestID request_id,
                                   HostHandle host_handle,
                                   std::unique_ptr<ProtoMessage> reply)
    : client_id_(client_id),
      request_id_(request_id),
      host_handle_(std::move(host_handle)),
      reply_(std::move(reply)) {
  DCHECK(reply_);
}

ServiceReplyBase::~ServiceReplyBase() {
  if (reply_)
    Abort();
}

ServiceReplyBase::ServiceReplyBase(ServiceReplyBase&&) noexcept = default;
ServiceReplyBase& ServiceReplyBase::operator=(ServiceReplyBase&&) = default;

void ServiceReplyBase::Abort() {
  if (!reply_) {
    DCHECK(false);
    return;
  }
  reply_.reset();
  host_handle_.ReplyToMethodInvocation(client_id_, request_id_, nullptr);
}

void ServiceReplyBase::Send() {
  if (!reply_) {
    DCHECK(false);
    return;
  }
  host_handle_.ReplyToMethodInvocation(client_id_, request_id_,
                                       std::move(reply_));
}

}  // namespace protorpc
}  // namespace perfetto
