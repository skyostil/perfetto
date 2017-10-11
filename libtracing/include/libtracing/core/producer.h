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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_CORE_PRODUCER_H_
#define LIBTRACING_INCLUDE_LIBTRACING_CORE_PRODUCER_H_

#include "libtracing/core/basic_types.h"

namespace perfetto {

class DataSourceConfig;

// This is just an interface. There are two classes deriving this:
//  1. The unix rpc implementation in src/unix_rpc/unix_service_impl.cc
//     to create a proxy that forwards requests via the unix socket.
//  2. The actual producer code, defined by the libtracing client
//     (see TestProducer in src/uinx_rpc/unix_test.cc as an example).
class Producer {
 public:
  virtual ~Producer() {}

  // DataSourceInstanceID is a random number chosen by the Service that the
  // Producer has to keep around (used to match the data source instance in
  // TearDownDataSourceInstance()).
  virtual void CreateDataSourceInstance(const DataSourceConfig&,
                                        DataSourceInstanceID) = 0;

  virtual void TearDownDataSourceInstance(DataSourceInstanceID) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_PRODUCER_H_
