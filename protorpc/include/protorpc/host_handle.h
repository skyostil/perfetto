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

#ifndef PROTORPC_INCLUDE_PROTORPC_HOST_HANDLE_H_
#define PROTORPC_INCLUDE_PROTORPC_HOST_HANDLE_H_

#include <memory>

#include "protorpc/basic_types.h"

namespace perfetto {
namespace protorpc {

class Host;

// Decouples the lifetime of the Host with the lifetime of the Service(s),
// allowing to no-op replies if the Host is destroyed while a Service is still
// alive. It's a weakptr essentially.
class HostHandle {
 public:
  explicit HostHandle(Host* host);
  ~HostHandle();
  HostHandle(HostHandle&&) noexcept;
  HostHandle& operator=(HostHandle&&);

  void ReplyToMethodInvocation(ClientID,
                               RequestID,
                               std::unique_ptr<ProtoMessage>);

  // Called by the Host dtor.
  void clear_host() { host_ = nullptr; }

 private:
  HostHandle(const HostHandle&) = delete;
  HostHandle& operator=(const HostHandle&) = delete;

  Host* host_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_HOST_HANDLE_H_
