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

#include "protorpc/service_request.h"

#include "cpp_common/base.h"
#include "google/protobuf/message_lite.h"

namespace perfetto {
namespace protorpc {

ServiceRequestBase::ServiceRequestBase(std::unique_ptr<ProtoMessage> request)
    : request_(std::move(request)) {}

ServiceRequestBase::~ServiceRequestBase() = default;

}  // namespace protorpc
}  // namespace perfetto
