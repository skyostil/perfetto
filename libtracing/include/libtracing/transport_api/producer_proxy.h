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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_PRODUCER_PROXY_H_
#define LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_PRODUCER_PROXY_H_

#include <memory>

#include "libtracing/core/basic_types.h"

namespace perfetto {

// The interface that the transport layer has to implement in order to model the
// transport in the [service implementation] -> [remote producer] direction.
// The transport must override the virtual methods below and turn them into
// remote procedure calls.
class ProducerProxy {
 public:
  virtual ~ProducerProxy() {}

  virtual unique_ptr<SharedMemory> CreateSharedMemory(size_t) = 0;
  virtual void CreateDataSourceInstance(DataSourceInstanceID,
                                        const DataSourceConfig&)= 0;
  virtual void TearDownDataSourceInstance(DataSourceInstanceID) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_TRANSPORT_API_PRODUCER_PROXY_H_
