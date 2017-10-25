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

#ifndef PROTORPC_INCLUDE_PROTORPC_SERVICE_REQUEST_H_
#define PROTORPC_INCLUDE_PROTORPC_SERVICE_REQUEST_H_

#include <memory>
#include <utility>

#include "protorpc/basic_types.h"

namespace perfetto {
namespace protorpc {

class ServiceRequestBase {
 public:
  explicit ServiceRequestBase(std::unique_ptr<ProtoMessage> request);

  ProtoMessage* request() const { return request_.get(); }

 private:
  ServiceRequestBase(const ServiceRequestBase&) = delete;
  ServiceRequestBase& operator=(const ServiceRequestBase&) = delete;

  std::unique_ptr<ProtoMessage> request_;
};

template <typename T>
class ServiceRequest : public ServiceRequestBase {
 public:
  explicit ServiceRequest(std::unique_ptr<T> request)
      : ServiceRequestBase(std::move(request)) {
    static_assert(std::is_base_of<ProtoMessage, T>::value,
                  "T must be a ProtoMessage");
  }
  T& operator*() { return static_cast<T&>(*request()); }
  T* operator->() { return static_cast<T*>(request()); }
};

}  // namespace perfetto
}  // namespace protorpc

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_REQUEST_H_
