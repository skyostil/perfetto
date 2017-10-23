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
#include "libtracing/core/producer.h"
#include "libtracing/core/shared_memory.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"

namespace perfetto {

// TODO add ThreadChecker everywhere.

// TODO Maybe add a test in the ctor that checks that the implementation of the
// TaskRunner doesn't short-circuit calls by just invoking the tasks inline,
// as that would cause very subtle re-entrancy bugs.

namespace {
constexpr size_t kShmSize = 4096;
}  // namespace

// static
std::unique_ptr<Service> Service::CreateInstance(
    std::unique_ptr<SharedMemory::Factory> shm_factory,
    TaskRunner* task_runner) {
  return std::unique_ptr<Service>(
      new ServiceImpl(std::move(shm_factory), task_runner));
}

ServiceImpl::ServiceImpl(std::unique_ptr<SharedMemory::Factory> shm_factory,
                         TaskRunner* task_runner)
    : shm_factory_(std::move(shm_factory)), task_runner_(task_runner) {
  DCHECK(task_runner_);
}

ServiceImpl::~ServiceImpl() {
  CHECK(false);  // TODO handle teardown of all Producer?
}

Service::ProducerEndpoint* ServiceImpl::ConnectProducer(
    std::unique_ptr<Producer> producer) {
  const ProducerID id = ++last_producer_id_;
  auto shared_memory = shm_factory_->CreateSharedMemory(kShmSize);
  std::unique_ptr<ProducerEndpointImpl> new_endpoint(new ProducerEndpointImpl(
      id, task_runner_, std::move(producer), std::move(shared_memory)));
  auto it_and_inserted = producers_.emplace(id, std::move(new_endpoint));
  if (!it_and_inserted.second) {
    DCHECK(false);
    return nullptr;
  }

  ProducerEndpointImpl* endpoint = it_and_inserted.first->second.get();
  task_runner_->PostTask(std::bind(&Producer::OnConnect, endpoint->producer(),
                                   id, endpoint->shared_memory()));
  return endpoint;
}

void ServiceImpl::DisconnectProducer(Service::ProducerEndpoint* endpoint) {
  DCHECK(producers_.count(endpoint->GetID()));
  producers_.erase(endpoint->GetID());
  // TODO still has to tear down some resources.
}

void ServiceImpl::CreateDataSourceInstanceForTesting(
    ProducerID prid,
    const DataSourceConfig& config) {
  auto producer_it = producers_.find(prid);
  CHECK(producer_it != producers_.end());
  last_data_source_instance_id_ += 10;
  DataSourceInstanceID dsid = last_data_source_instance_id_;
  producer_it->second->producer()->CreateDataSourceInstance(dsid, config);
}

////////////////////////////////////////////////////////////////////////////////
// ServiceImpl::ProducerEndpointImpl implementation
////////////////////////////////////////////////////////////////////////////////

ServiceImpl::ProducerEndpointImpl::ProducerEndpointImpl(
    ProducerID id,
    TaskRunner* task_runner,
    std::unique_ptr<Producer> producer,
    std::unique_ptr<SharedMemory> shared_memory)
    : id_(id),
      task_runner_(task_runner),
      producer_(std::move(producer)),
      shared_memory_(std::move(shared_memory)) {}

ServiceImpl::ProducerEndpointImpl::~ProducerEndpointImpl() {}

ProducerID ServiceImpl::ProducerEndpointImpl::GetID() const {
  return id_;
}

// Service::ProducerEndpoint implementation.
void ServiceImpl::ProducerEndpointImpl::RegisterDataSource(
    const DataSourceDescriptor& desc,
    RegisterDataSourceCallback callback) {
  DLOG("[ServiceImpl] RegisterDataSource from producer id=%" PRIu64, id_);
  task_runner_->PostTask(
      std::bind(std::move(callback), ++last_data_source_id_));
}

void ServiceImpl::ProducerEndpointImpl::UnregisterDataSource(
    DataSourceID dsid) {
  CHECK(dsid);
  return;
}

void ServiceImpl::ProducerEndpointImpl::NotifyPageAcquired(uint32_t page) {
  DLOG("[ServiceImpl] NotifyPageAcquired from producer id=%" PRIu64, id_);
  return;
}

void ServiceImpl::ProducerEndpointImpl::NotifyPageReleased(uint32_t page) {
  DLOG("[ServiceImpl] NotifyPageReleased from producer id=%" PRIu64, id_);
  DCHECK(shared_memory_);
  DLOG("[ServiceImpl] Reading Shared memory: \"%s\"",
       reinterpret_cast<const char*>(shared_memory_->start()));
  return;
}

}  // namespace perfetto
