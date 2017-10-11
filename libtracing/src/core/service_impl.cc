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
#include "libtracing/core/task_runner_proxy.h"
#include "libtracing/src/core/base.h"

namespace perfetto {

// TODO add thread checks.

// TODO What if the implementation of the embedder might shortcircuit the
// PostTask and they might re-enter?

ServiceImpl::ServiceImpl(Delegate* delegate) : delegate_(delegate) {}

ServiceImpl::~ServiceImpl() {}

void ServiceImpl::ConnectProducer(std::unique_ptr<Producer> producer,
                                  ConnectProducerCallback cb) {
  ProducerID id = ++last_producer_id_;
  Producer* ptr = producer.get();
  producers_[id] = std::move(producer);
  delegate_->task_runner()->PostTask(std::bind(cb, ptr, id));
  // TODO what guarantees that the producers_[id] isn't deleted between here and
  // when the task is actually posted? That migh cause the callback receiver
  // to hit an invalid pointer and casuse a UAF.
}

void ServiceImpl::RegisterDataSource(ProducerID prid,
                                     const DataSourceDescriptor& desc,
                                     RegisterDataSourceCallback cb) {
  CHECK(prid);
  DLOG("[ServiceImpl] RegisterDataSource from producer id=%" PRIu64 "\n", prid);
  delegate_->task_runner()->PostTask(std::bind(cb, prid * 10));
}

void ServiceImpl::UnregisterDataSource(DataSourceID dsid) {
  CHECK(dsid);
  return;
}

void ServiceImpl::NotifyPageTaken(ProducerID prid, uint32_t page_index) {
  CHECK(prid);
  DLOG("[ServiceImpl] NotifyPageTaken from producer id=%" PRIu64 "\n", prid);
  return;
}

void ServiceImpl::NotifyPageReleased(ProducerID prid, uint32_t page_index) {
  CHECK(prid);
  DLOG("[ServiceImpl] NotifyPageReleased from producer id=%" PRIu64 "\n", prid);
  return;
}

void ServiceImpl::CreateDataSourceInstanceForTesting(
    const DataSourceConfig& config,
    DataSourceInstanceID dsid) {
  DCHECK(!producers_.empty());
  Producer* random_producer = producers_.begin()->second.get();
  random_producer->CreateDataSourceInstance(config, dsid);
}

}  // namespace perfetto
