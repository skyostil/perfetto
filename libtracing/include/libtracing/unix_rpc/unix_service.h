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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_H_
#define LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_H_

#include <memory>

#include "libtracing/core/service.h"

namespace perfetto {

class DataSourceConfig;
class TaskRunnerProxy;

// At first might be a little contra-intuitive that this class does not inherit
// from Service (from /include/core/service.h).
// The instance we return here doesn't need to expose any method other than the
// factory method. Clients are just suposed to create and teardown the service
// to host it in their process. They are not supposed to poke with the Service
// methods, that are meant to be accessed only via RPC.

class UnixService {
 public:
  // Implemented by the libtracing client.
  class Delegate {
   public:
    virtual ~Delegate() {}

    virtual TaskRunnerProxy* task_runner() const = 0;
    virtual void OnDataSourceConnected(DataSourceID) = 0;
  };

  static std::unique_ptr<UnixService> CreateInstance(const char* socket_name,
                                                     Delegate*);

  virtual ~UnixService() {}

  // Temporary, just for testing, because the Consumer+Config port is not yet
  // implemented. This method essentially simulates a configuration change
  // from the Consumer, as if we had one connected.
  virtual void CreateDataSourceInstanceForTesting(const DataSourceConfig&,
                                                  DataSourceInstanceID) = 0;

  virtual bool Start() = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_H_
