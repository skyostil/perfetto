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
#include "libtracing/core/shared_memory.h"

namespace perfetto {

class DataSourceConfig;
class DataSourceDescriptor;
class Producer;
class TaskRunner;

// The public API of the tracing service business logic.
//
// Exposed to:
//   1. The transport layer (e.g., src/unix_rpc/unix_service_host.cc),
//      which forwards commands received from a remote Producer or Consumer to
//     the actual service implementation.
//   2. Tests.
//
// Subclassed by:
//   The service business logic in src/core/service_impl.cc.
class Service {
 public:
  // Subclassed by:
  //   1. The service_impl.cc business logic when retuning it in response to
  //      the ConnectProducer() method.
  //   2. The transport layer (e.g., src/unix_rpc) when the producer and
  //      the service don't talk locally but via some RPC mechanism.
  class ProducerEndpoint {
   public:
    virtual ~ProducerEndpoint() {}

    virtual ProducerID GetID() const = 0;
    using RegisterDataSourceCallback = std::function<void(DataSourceID)>;
    virtual void RegisterDataSource(const DataSourceDescriptor&,
                                    RegisterDataSourceCallback) = 0;
    virtual void UnregisterDataSource(DataSourceID) = 0;

    virtual void NotifyPageAcquired(uint32_t page_index) = 0;
    virtual void NotifyPageReleased(uint32_t page_index) = 0;
  };  // class ProducerEndpoint.

  static std::unique_ptr<Service> CreateInstance(
      std::unique_ptr<SharedMemory::Factory>,
      TaskRunner*);

  virtual ~Service() {}

  // The passed ProducerProxy will be kept alive at least until the call to
  // DisconnectProducer()
  virtual ProducerEndpoint* ConnectProducer(std::unique_ptr<Producer>) = 0;

  // The ProducerEndpoint* returned by the corresponding ConnectProducer() call
  // is no longer valid after this call.
  virtual void DisconnectProducer(ProducerEndpoint*) = 0;

  virtual void CreateDataSourceInstanceForTesting(ProducerID,
                                                  const DataSourceConfig&) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_SERVICE_H_
