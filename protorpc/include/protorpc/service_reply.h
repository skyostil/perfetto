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

#ifndef PROTORPC_INCLUDE_PROTORPC_SERVICE_REPLY_H_
#define PROTORPC_INCLUDE_PROTORPC_SERVICE_REPLY_H_

#include <memory>
#include <utility>

#include "protorpc/basic_types.h"
#include "protorpc/host_handle.h"

namespace perfetto {
namespace protorpc {

class ServiceReplyBase {
 public:
  ServiceReplyBase(ClientID,
                   RequestID,
                   HostHandle,
                   std::unique_ptr<ProtoMessage> reply);
  virtual ~ServiceReplyBase();
  ServiceReplyBase(ServiceReplyBase&&) noexcept;
  ServiceReplyBase& operator=(ServiceReplyBase&&);

  void Abort();
  void Send();

  ProtoMessage* reply() const { return reply_.get(); }

 private:
  ServiceReplyBase(const ServiceReplyBase&) = delete;
  ServiceReplyBase& operator=(const ServiceReplyBase&) = delete;

  ClientID client_id_;
  RequestID request_id_;
  HostHandle host_handle_;
  std::unique_ptr<ProtoMessage> reply_;
};

template <typename T>
class ServiceReply : public ServiceReplyBase {
 public:
  ServiceReply(ClientID client_id, RequestID request_id, HostHandle host_handle)
      : ServiceReplyBase(client_id,
                         request_id,
                         std::move(host_handle),
                         std::unique_ptr<ProtoMessage>(new T())) {
    static_assert(std::is_base_of<ProtoMessage, T>::value,
                  "T must be a ProtoMessage");
  }

  ServiceReply(ServiceReply&&) noexcept = default;
  ServiceReply& operator=(ServiceReply&&) = default;

  T& operator*() { return static_cast<T&>(*reply()); }
  T* operator->() { return static_cast<T*>(reply()); }
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_REPLY_H_
