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

#ifndef PROTORPC_INCLUDE_PROTORPC_SERVICE_H_
#define PROTORPC_INCLUDE_PROTORPC_SERVICE_H_

namespace perfetto {
namespace protorpc {

class ServiceDescriptor;

// TODO do we need this class at all?
class Service {
 public:
  virtual ~Service() = default;
  virtual const ServiceDescriptor& GetDescriptor() = 0;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_SERVICE_H_
