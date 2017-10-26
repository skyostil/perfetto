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

// TODO(primiano): protobuf leaks CHECK macro, sigh.
#include "wire_protocol.pb.h"

#include "protorpc/host_handle.h"

#include <utility>

#include "google/protobuf/message_lite.h"
#include "protorpc/host.h"

namespace perfetto {
namespace protorpc {

HostHandle::HostHandle(Host* host) : host_(host) {
  host_->AddHandle(this);
}

HostHandle::~HostHandle() = default;

HostHandle::HostHandle(HostHandle&& other) noexcept {
  *this = std::move(other);
}

HostHandle& HostHandle::operator=(HostHandle&& other) {
  if (host_)
    host_->RemoveHandle(this);
  host_ = other.host_;
  host_->RemoveHandle(&other);
  other.host_ = nullptr;
  host_->AddHandle(this);
  return *this;
}

void HostHandle::ReplyToMethodInvocation(ClientID client_id,
                                         RequestID request_id,
                                         std::unique_ptr<ProtoMessage> reply) {
  if (host_)
    host_->ReplyToMethodInvocation(client_id, request_id, std::move(reply));
}

}  // namespace protorpc
}  // namespace perfetto
