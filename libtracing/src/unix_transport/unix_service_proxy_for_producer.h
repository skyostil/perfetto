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

#ifndef LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_PROXY_FOR_PRODUCER_H_
#define LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_PROXY_FOR_PRODUCER_H_

#include "libtracing/core/basic_types.h"
#include "libtracing/src/unix_transport/unix_socket.h"
#include "libtracing/transport/service_proxy_for_producer.h"

namespace perfetto {

class Producer;
class TaskRunner;
class UnixSharedMemory;

// Implements the transport ServiceProxyForProducer interface, via RPC over a
// UNIX socket.
class UnixServiceProxyForProducer : public ServiceProxyForProducer {
 public:
  UnixServiceProxyForProducer(Producer*, TaskRunner*);
  ~UnixServiceProxyForProducer() override;

  bool Connect(const char* service_socket_name);

  // ServiceProxyForProducer implementation.
  void RegisterDataSource(const DataSourceDescriptor&,
                          RegisterDataSourceCallback) override;
  void UnregisterDataSource(DataSourceID) override;
  SharedMemory* GetSharedMemory() override;
  void NotifyPageAcquired(uint32_t page_index) override;
  void NotifyPageReleased(uint32_t page_index) override;

 private:
  void OnDataAvailable();

  Producer* const producer_;
  TaskRunner* const task_runner_;
  RegisterDataSourceCallback pending_register_data_source_callback_;
  UnixSocket conn_;
  std::unique_ptr<UnixSharedMemory> shmem_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_PROXY_FOR_PRODUCER_H_
