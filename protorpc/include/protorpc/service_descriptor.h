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

#include "protorpc/basic_types.h"
#include "protorpc/deferred.h"

namespace perfetto {
namespace protorpc {

class Service;

class ServiceDescriptor {
 public:
  struct Method {
    const char* name;

    using DecoderFunc = std::unique_ptr<ProtoMessage> (*)(const std::string&);
    DecoderFunc request_proto_decoder;
    DecoderFunc reply_proto_decoder;

    using NewReplyFunc = std::unique_ptr<ProtoMessage> (*)(void);
    NewReplyFunc reply_proto_factory;

    using InvokerFunc = void (*)(Service*,
                                 const ProtoMessage&,
                                 Deferred<ProtoMessage>);
    InvokerFunc invoker;
  };

  std::string service_name;

  // Note that methods order is not stable. Client and Host might have different
  // method numbers, depending on their versions, so the Client can't just rely
  // on the indexes and has to keep a translation map locally, see ServiceProxy.
  std::vector<Method> methods;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_DESCRIPTOR_H_
