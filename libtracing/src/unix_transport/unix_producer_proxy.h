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

#ifndef LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_PRODUCER_PROXY_H_
#define LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_PRODUCER_PROXY_H_

#include "libtracing/core/basic_types.h"
#include "libtracing/src/unix_transport/unix_socket.h"
#include "libtracing/transport/producer_proxy.h"
#include "libtracing/unix_transport/unix_service_host.h"

namespace perfetto {

class Service;
class TaskRunner;
class UnixSharedMemory;

class UnixProducerProxy : public ProducerProxy {
 public:
  UnixProducerProxy(Service*,
                    UnixSocket,
                    TaskRunner*,
                    UnixServiceHost::ObserverForTesting*);
  ~UnixProducerProxy() override;

  void SendSharedMemory(const UnixSharedMemory&);
  void OnDataAvailable();

  void SetId(ProducerID id);

  UnixSocket* conn() { return &conn_; }

  // ProducerProxy implementation.
  std::unique_ptr<SharedMemory> InitializeSharedMemory(
      size_t shm_size) override;
  void CreateDataSourceInstance(DataSourceInstanceID,
                                const DataSourceConfig&) override;
  void TearDownDataSourceInstance(DataSourceInstanceID) override;

 private:
  UnixProducerProxy(const UnixProducerProxy&) = delete;
  UnixProducerProxy& operator=(const UnixProducerProxy&) = delete;

  ProducerID id_ = 0;
  Service* const svc_;
  UnixSocket conn_;
  TaskRunner* const task_runner_;
  UnixServiceHost::ObserverForTesting* const observer_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_PRODUCER_PROXY_H_
\
