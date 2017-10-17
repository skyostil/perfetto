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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_H_
#define LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_H_

#include <stdint.h>

#include <functional>

#include "libtracing/core/basic_types.h"

namespace perfetto {

// The interface that the transport layer has to implement in order to model the
// transport in the [producer implementation] -> [remote service] direction.
// The transport must override the virtual methods below and turn them into
// remote procedure calls.
class ServiceProxy {
 public:
  virtual ~ServiceProxy() {}

  using RegisterDataSourceCallback = std::function<void(DataSourceID)>;
  virtual void RegisterDataSource(const DataSourceDescriptor&,
                                  RegisterDataSourceCallback) = 0;
  virtual void UnregisterDataSource(DataSourceID) = 0;
  virtual void NotifyPageTaken(uint32_t page_index) = 0;
  virtual void NotifyPageReleased(uint32_t page_index) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_H_
