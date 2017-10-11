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

class Producer;
class DataSourceDescriptor;
class SharedMemory;

// This is just an interface. There are two classes deriving this:
//  1. The RPC-agnostic machinery implementation in src/core/service_impl.cc
//  2. The RPC stub for the client in src/unix_rpc/unix_producer.cc .
class Service {
 public:
  virtual ~Service() {}

  using ConnectProducerCallback =
      std::function<void(Producer* producer, ProducerID)>;
  virtual void ConnectProducer(std::unique_ptr<Producer>,
                               ConnectProducerCallback) = 0;

  using RegisterDataSourceCallback = std::function<void(DataSourceID)>;
  virtual void RegisterDataSource(ProducerID,
                                  const DataSourceDescriptor&,
                                  RegisterDataSourceCallback) = 0;

  virtual void UnregisterDataSource(DataSourceID) = 0;

  virtual void NotifyPageTaken(ProducerID, uint32_t page_index) = 0;
  virtual void NotifyPageReleased(ProducerID, uint32_t page_index) = 0;

 protected:
  Service() = default;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_SERVICE_H_
