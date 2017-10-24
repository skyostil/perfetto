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

#ifndef TRACING_SRC_CORE_SERVICE_IMPL_H_
#define TRACING_SRC_CORE_SERVICE_IMPL_H_

#include <functional>
#include <map>
#include <memory>

#include "tracing/core/basic_types.h"
#include "tracing/core/service.h"

namespace perfetto {

class DataSourceConfig;
class Producer;
class SharedMemory;
class TaskRunner;

// The tracing service business logic.
class ServiceImpl : public Service {
 public:
  explicit ServiceImpl(std::unique_ptr<SharedMemory::Factory>, TaskRunner*);
  ~ServiceImpl() override;

  Service::ProducerEndpoint* ConnectProducer(
      std::unique_ptr<Producer>) override;
  void DisconnectProducer(Service::ProducerEndpoint*) override;
  void CreateDataSourceInstanceForTesting(ProducerID,
                                          const DataSourceConfig&) override;

 private:
  // The service endpoint exposed to each producer.
  class ProducerEndpointImpl : public Service::ProducerEndpoint {
   public:
    ProducerEndpointImpl(ProducerID,
                         TaskRunner*,
                         std::unique_ptr<Producer>,
                         std::unique_ptr<SharedMemory>);
    ~ProducerEndpointImpl() override;

    Producer* producer() const { return producer_.get(); }
    SharedMemory* shared_memory() const { return shared_memory_.get(); }

    // Service::ProducerEndpoint implementation.
    ProducerID GetID() const override;
    void RegisterDataSource(const DataSourceDescriptor&,
                            RegisterDataSourceCallback) override;
    void UnregisterDataSource(DataSourceID) override;

    void NotifyPageAcquired(uint32_t page) override;
    void NotifyPageReleased(uint32_t page) override;

   private:
    ProducerEndpointImpl(const ProducerEndpointImpl&) = delete;
    ProducerEndpointImpl& operator=(const ProducerEndpointImpl&) = delete;

    ProducerID const id_;
    TaskRunner* const task_runner_;
    std::unique_ptr<Producer> producer_;
    std::unique_ptr<SharedMemory> shared_memory_;
    DataSourceID last_data_source_id_ = 0;
  };

  ServiceImpl(const ServiceImpl&) = delete;
  ServiceImpl& operator=(const ServiceImpl&) = delete;

  std::unique_ptr<SharedMemory::Factory> shm_factory_;
  TaskRunner* const task_runner_;
  ProducerID last_producer_id_ = 0;
  DataSourceInstanceID last_data_source_instance_id_ = 0;
  std::map<ProducerID, std::unique_ptr<ProducerEndpointImpl>> producers_;
};

}  // namespace perfetto

#endif  // TRACING_SRC_CORE_SERVICE_IMPL_H_
