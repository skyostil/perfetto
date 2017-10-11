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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_
#define LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_

#include <memory>

namespace perfetto {

class Producer;
class Service;
class TaskRunnerProxy;

// The client-side of UnixService. Allows to connect to an existing service
// instance over a UNIX socket and exposes the Producer/Service API proxying
// them over RPC.
class UnixServiceConnection {
 public:
  // Connects to the producer port of the Service listening on the given
  // |service_socket_name|. Returns a RPC proxy interface that allows to
  // interact with the service if the connection is succesfully, or nullptr if
  // the service is unreachable.
  static std::unique_ptr<Service> ConnectAsProducer(
      const char* service_socket_name,
      Producer*,
      TaskRunnerProxy*);

  // Not implemented yet.
  // static std::unique_ptr<Service> ConnectAsConsumer(Producer*);

 private:
  UnixServiceConnection() = delete;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_
