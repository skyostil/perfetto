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

#ifndef LIBTRACING_SRC_CORE_SERVICE_IMPL_H_
#define LIBTRACING_SRC_CORE_SERVICE_IMPL_H_

#include <functional>
#include <map>
#include <memory>

#include "libtracing/core/basic_types.h"
#include "libtracing/core/service.h"
#include "libtracing/core/shared_memory.h"

namespace perfetto {

class DataSourceConfig;
class ProducerProxy;
class TaskRunner;

// The tracing service business logic. Encapsulated by the transport layer
// (e.g., src/unix_transport/unix_service_host.cc).
class ServiceImpl : public Service {
 public:
  explicit ServiceImpl(TaskRunner*);
  ~ServiceImpl() override;

  ProducerID ConnectProducer(std::unique_ptr<ProducerProxy>) override;
  void DisconnectProducer(ProducerID) override;

  DataSourceID RegisterDataSource(ProducerID,
                                  const DataSourceDescriptor&) override;
  void UnregisterDataSource(ProducerID, DataSourceID) override;

  SharedMemory* GetSharedMemoryForProducer(ProducerID) override;
  void NotifyPageAcquired(ProducerID, uint32_t page_index) override;
  void NotifyPageReleased(ProducerID, uint32_t page_index) override;

  DataSourceInstanceID CreateDataSourceInstanceForTesting(
      ProducerID,
      const DataSourceConfig&) override;

 private:
  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;

  TaskRunner* const task_runner_;
  ProducerID last_producer_id_ = 0;
  DataSourceID last_data_source_id_ = 0;
  DataSourceInstanceID last_data_source_instance_id_ = 0;
  std::map<ProducerID, std::unique_ptr<ProducerProxy>> producers_;
  std::map<ProducerID, std::unique_ptr<SharedMemory>> producer_shm_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_CORE_SERVICE_IMPL_H_
