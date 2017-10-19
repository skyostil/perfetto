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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_FOR_PRODUCER_H_
#define LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_FOR_PRODUCER_H_

#include <stdint.h>

#include <functional>

#include "libtracing/core/basic_types.h"

namespace perfetto {

class DataSourceDescriptor;
class SharedMemory;

// Exposed to:
//   producer(s), the actual code in the clients of libtracing that wants to
//   connect and interact with the service.
//
// Subclassed by:
//   the transport layer (e.g., src/unix_transport) that proxies requests
//   between Producer and Service over some RPC mechanism.
class ServiceProxyForProducer {
 public:
  virtual ~ServiceProxyForProducer() {}

  using RegisterDataSourceCallback = std::function<void(DataSourceID)>;
  virtual void RegisterDataSource(const DataSourceDescriptor&,
                                  RegisterDataSourceCallback) = 0;
  virtual void UnregisterDataSource(DataSourceID) = 0;

  virtual SharedMemory* GetSharedMemory() = 0;
  virtual void NotifyPageAcquired(uint32_t page_index) = 0;
  virtual void NotifyPageReleased(uint32_t page_index) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_SERVICE_PROXY_FOR_PRODUCER_H_
