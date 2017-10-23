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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_HOST_H_
#define LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_HOST_H_

#include <memory>

#include "libtracing/core/basic_types.h"

namespace perfetto {

class DataSourceConfig;
class Service;
class TaskRunner;

// Creates an instance of the service (business logic + UNIX socket transport).
// Exposed to:
//   The code in the libtracing client that will host the service e.g., traced.
// Implemented in:
//   src/unix_rpc/unix_service_host.cc
class UnixServiceHost {
 public:
  class ObserverForTesting {
   public:
    virtual ~ObserverForTesting() {}

    // TODO: implement all these in unix_service_host_impl.cc.
    virtual void OnProducerConnected(ProducerID) = 0;
    virtual void OnDataSourceRegistered(DataSourceID) = 0;
    virtual void OnDataSourceUnregistered(DataSourceID) = 0;
    virtual void OnDataSourceInstanceCreated(DataSourceInstanceID) = 0;
    virtual void OnDataSourceInstanceDestroyed(DataSourceInstanceID) = 0;
  };

  static std::unique_ptr<UnixServiceHost> CreateInstance(
      const char* socket_name,
      TaskRunner*,
      ObserverForTesting* = nullptr);
  virtual ~UnixServiceHost() {}

  // Start listening on the Producer & Consumer ports. Returns false in case of
  // failure (e.g., something else is listening on |socket_name|).
  virtual bool Start() = 0;

  // Accesses the underlying Service business logic. Exposed only for testing.
  virtual Service* service_for_testing() const = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_HOST_H_
