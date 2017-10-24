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

#ifndef TRACING_INCLUDE_TRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_
#define TRACING_INCLUDE_TRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_

#include <memory>

#include "tracing/core/service.h"

namespace perfetto {

class Producer;
class ServiceProxyForProducer;
class TaskRunner;

// Allows to connect to an existing service through a UNIX doamain socket.
// Exposed to:
//   Producer(s) and Consumer(s) in the tracing clients.
// Implemented in:
//   src/unix_rpc/unix_service_connection.cc
class UnixServiceConnection {
 public:
  // Connects to the producer port of the Service listening on the given
  // |service_socket_name|. Returns a ProducerEndpoint interface that allows to
  // interact with the service if the connection is succesful, or nullptr if
  // the service is unreachable.
  static std::unique_ptr<Service::ProducerEndpoint>
  ConnectAsProducer(const char* service_socket_name, Producer*, TaskRunner*);

  // Not implemented yet.
  // static std::unique_ptr<ServiceProxy> ConnectAsConsumer(Producer*);

 private:
  UnixServiceConnection() = delete;
};

}  // namespace perfetto

#endif  // TRACING_INCLUDE_TRACING_UNIX_RPC_UNIX_SERVICE_CONNECTION_H_
