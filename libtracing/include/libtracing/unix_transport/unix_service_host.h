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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_UNIX_TRANSPORT_UNIX_SERVICE_HOST_H_
#define LIBTRACING_INCLUDE_LIBTRACING_UNIX_TRANSPORT_UNIX_SERVICE_HOST_H_

#include <memory>

#include "libtracing/core/basic_types.h"

namespace perfetto {

class DataSourceConfig;
class TaskRunnerProxy;

// Creates an instance of the service (business logic + UNIX socket transport).
// This is meant to be used in the process that will host the tracing service.
// The concrete implementation of this class lives in src/unix_transport/.
class UnixServiceHost {
 public:
  // Implemented by the libtracing client (e.g., src/unix_rpc/unix_test.cc).
  // Used only for tests.
  class ObserverForTesting {
   public:
    virtual ~ObserverForTesting() {}

    virtual void OnProducerConnected(ProducerID) = 0;
    virtual void OnDataSourceRegistered(DataSourceID) = 0;
    virtual void OnDataSourceUnregistered(DataSourceID) = 0;
    virtual void OnDataSourceInstanceCreated(DataSourceInstanceID) = 0;
    virtual void OnDataSourceInstanceDestroyed(DataSourceInstanceID) = 0;
  };

  static std::unique_ptr<UnixServiceHost> CreateInstance(
      const char* socket_name,
      TaskRunnerProxy*,
      ObserverForTesting* = nullptr);

  virtual ~UnixServiceHost() {}

  // Temporary, just for testing, because the Consumer+Config port is not yet
  // implemented. This method essentially simulates a configuration change
  // from an hypotetical Consumer.
  void CreateDataSourceInstanceForTesting(const DataSourceConfig&,
                                          DataSourceInstanceID);

  bool Start() = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_UNIX_TRANSPORT_UNIX_SERVICE_HOST_H_
