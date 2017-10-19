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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_CORE_SERVICE_H_
#define LIBTRACING_INCLUDE_LIBTRACING_CORE_SERVICE_H_

#include <functional>
#include <memory>

#include "libtracing/core/basic_types.h"

namespace perfetto {

class DataSourceConfig;
class DataSourceDescriptor;
class ProducerProxy;
class SharedMemory;
class TaskRunner;

// The public API of the tracing service business logic.
//
// Exposed to:
//   1. The transport layer (e.g., src/unix_transport/unix_service_host.cc),
//      which forwards commands received from a remote Producer or Consumer to
//     the actual service implementation.
//   2. Tests.
//
// Subclassed by:
//   The service business logic in src/core/service_impl.cc.
class Service {
 public:
  static std::unique_ptr<Service> CreateInstance(TaskRunner*);

  virtual ~Service() {}

  // The passed ProducerProxy will be kept alive at least until the call to
  // DisconnectProducer()
  virtual ProducerID ConnectProducer(std::unique_ptr<ProducerProxy>) = 0;
  virtual void DisconnectProducer(ProducerID) = 0;

  virtual DataSourceID RegisterDataSource(ProducerID,
                                          const DataSourceDescriptor&) = 0;
  virtual void UnregisterDataSource(ProducerID, DataSourceID) = 0;

  virtual SharedMemory* GetSharedMemoryForProducer(ProducerID) = 0;
  virtual void NotifyPageAcquired(ProducerID, uint32_t page_index) = 0;
  virtual void NotifyPageReleased(ProducerID, uint32_t page_index) = 0;

  virtual DataSourceInstanceID CreateDataSourceInstanceForTesting(
      ProducerID,
      const DataSourceConfig&) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_SERVICE_H_
