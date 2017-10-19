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

#include "libtracing/src/core/service_impl.h"

#include <inttypes.h>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/shared_memory.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"
#include "libtracing/transport/producer_proxy.h"

namespace perfetto {

// TODO add ThreadChecker everywhere.

// TODO What if the implementation of the embedder might shortcircuit the
// PostTask and they might re-enter?

// static
std::unique_ptr<Service> Service::CreateInstance(TaskRunner* task_runner) {
  return std::unique_ptr<Service>(new ServiceImpl(task_runner));
}

ServiceImpl::ServiceImpl(TaskRunner* task_runner) : task_runner_(task_runner) {
  DCHECK(task_runner_);
}

ServiceImpl::~ServiceImpl() {
  CHECK(false);  // TODO handle teardown of all ProducerProxy?
}

ProducerID ServiceImpl::ConnectProducer(
    std::unique_ptr<ProducerProxy> producer) {
  ProducerID id = ++last_producer_id_;
  producer_shm_[id] = producer->InitializeSharedMemory(4096);
  DCHECK(producer_shm_[id]);
  producers_[id] = std::move(producer);
  return id;
}

void ServiceImpl::DisconnectProducer(ProducerID) {
  DCHECK(false);  // Not implemented.
}

DataSourceID ServiceImpl::RegisterDataSource(ProducerID prid,
                                             const DataSourceDescriptor& desc) {
  CHECK(prid);
  DLOG("[ServiceImpl] RegisterDataSource from producer id=%" PRIu64 "\n", prid);
  last_data_source_id_ += 1000;
  return last_data_source_id_;
}

void ServiceImpl::UnregisterDataSource(ProducerID prid, DataSourceID dsid) {
  CHECK(prid);
  CHECK(dsid);
  return;
}

void ServiceImpl::NotifyPageAcquired(ProducerID prid, uint32_t page_index) {
  CHECK(prid);
  DLOG("[ServiceImpl] NotifyPageAcquired from producer id=%" PRIu64 "\n", prid);
  return;
}

void ServiceImpl::NotifyPageReleased(ProducerID prid, uint32_t page_index) {
  CHECK(prid);
  DLOG("[ServiceImpl] NotifyPageReleased from producer id=%" PRIu64 "\n", prid);
  DCHECK(producer_shm_.count(prid) == 1);
  DLOG("[ServiceImpl] Reading Shared memory: \"%s\"\n",
       reinterpret_cast<const char*>(producer_shm_[prid]->start()));
  return;
}

SharedMemory* ServiceImpl::GetSharedMemoryForProducer(ProducerID prid) {
  auto producerid_and_shmem = producer_shm_.find(prid);
  if (producerid_and_shmem == producer_shm_.end())
    return nullptr;
  return producerid_and_shmem->second.get();
}

DataSourceInstanceID ServiceImpl::CreateDataSourceInstanceForTesting(
    ProducerID prid,
    const DataSourceConfig& config) {
  auto producer_it = producers_.find(prid);
  CHECK(producer_it != producers_.end());
  last_data_source_instance_id_ += 10;
  DataSourceInstanceID dsid = last_data_source_instance_id_;
  producer_it->second->CreateDataSourceInstance(dsid, config);
  return dsid;
}

}  // namespace perfetto
