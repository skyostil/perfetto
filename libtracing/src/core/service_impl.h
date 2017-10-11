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
class Producer;
class TaskRunnerProxy;

// The tracing service business logic. Embedders of this library are supposed to
// either encapsulate this, to wrap it with their own custom RPC mechanism
// (e.g., Chrome Mojo) or to use the UnixService wrapper.
class ServiceImpl : public Service {
 public:
  // The embedder (e.g., unix_rpc/unix_service_impl.h) is supposed to subclass
  // this Delegate interface to get platform abstractions.
  // TODO maybe this sholud be called PlatformDelegate, or Platform, just to
  // avoid calling everything just Delegate?
  class Delegate {
   public:
    virtual ~Delegate() {}
    virtual TaskRunnerProxy* task_runner() const = 0;
    virtual std::unique_ptr<SharedMemory> CreateSharedMemoryWithPeer(
        Producer* peer,
        size_t shm_size) = 0;
  };

  explicit ServiceImpl(Delegate*);
  ~ServiceImpl() override;

  void ConnectProducer(std::unique_ptr<Producer>,
                       ConnectProducerCallback) override;

  void RegisterDataSource(ProducerID,
                          const DataSourceDescriptor&,
                          RegisterDataSourceCallback) override;

  void UnregisterDataSource(DataSourceID) override;

  void NotifyPageTaken(ProducerID, uint32_t page_index) override;
  void NotifyPageReleased(ProducerID, uint32_t page_index) override;

  // Temporary, for testing.
  void CreateDataSourceInstanceForTesting(const DataSourceConfig&,
                                          DataSourceInstanceID);

 private:
  Delegate* const delegate_;
  ProducerID last_producer_id_ = 0;
  std::map<ProducerID, std::unique_ptr<Producer>> producers_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_CORE_SERVICE_IMPL_H_
