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

#include "libtracing/src/unix_transport/unix_service_proxy_for_producer.h"

#include <inttypes.h>
#include <stdio.h>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/producer.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/unix_transport/unix_shared_memory.h"

// TODO think to what happens when UnixServiceProxyForProducer gets destroyed
// w.r.t. the Producer pointer. Also think to lifetime of the Producer* during
// the callbacks.

namespace perfetto {

UnixServiceProxyForProducer::UnixServiceProxyForProducer(
    Producer* producer,
    TaskRunner* task_runner)
    : producer_(producer), task_runner_(task_runner) {}

UnixServiceProxyForProducer::~UnixServiceProxyForProducer() {}

bool UnixServiceProxyForProducer::Connect(const char* service_socket_name) {
  DCHECK(!conn_.is_connected());
  // TODO set short timeout here. maybe make async?
  if (!conn_.Connect(service_socket_name))
    return false;
  task_runner_->AddFileDescriptorWatch(
      conn_.fd(),
      std::bind(&UnixServiceProxyForProducer::OnDataAvailable, this));
  return true;
}

void UnixServiceProxyForProducer::OnDataAvailable() {
  DLOG("[unix_service_proxy_for_producer.cc] OnDataAvailable\n");
  char cmd[1024];
  int shm_fd = -1;  // TODO really need ScopedFD here.
  uint32_t shm_fd_size = 1;
  ssize_t rsize = conn_.Recv(cmd, sizeof(cmd) - 1, &shm_fd, &shm_fd_size);
  if (rsize <= 0) {
    task_runner_->RemoveFileDescriptorWatch(conn_.fd());
    return;  // TODO connection closed (very likely peer died. do something.
  }
  cmd[rsize] = '\0';

  if (strcmp(cmd, "SendSharedMemory") == 0) {
    DCHECK(shm_fd_size == 1);
    DCHECK(!shmem_);
    DLOG("[unix_service_proxy_for_producer.cc] Received shm, fd=%d\n", shm_fd);
    shmem_ = UnixSharedMemory::AttachToFd(shm_fd);
    DCHECK(shmem_);
    DLOG("[unix_service_proxy_for_producer.cc] Mapped shm, size=%lu\n",
         shmem_->size());

    // TODO what happens if Producer* goes away in the meantime?
    task_runner_->PostTask(std::bind(&Producer::OnConnect, producer_));
    return;
  }

  DataSourceID dsid;
  if (sscanf(cmd, "RegisterDataSourceCallback %" PRIu64, &dsid) == 1) {
    DCHECK(pending_register_data_source_callback_);
    task_runner_->PostTask(
        std::bind(std::move(pending_register_data_source_callback_), dsid));
    return;
  }

  DataSourceInstanceID inst_id;
  char data_source_name[128];
  char trace_filters[128];
  if (sscanf(cmd, "CreateDataSourceInstance %" PRIu64 " %128s %128s", &inst_id,
             data_source_name, trace_filters) == 3) {
    DataSourceConfig config{data_source_name, trace_filters};
    // TODO what happens if Producer* goes away in the meantime?
    task_runner_->PostTask(std::bind(&Producer::CreateDataSourceInstance,
                                     producer_, inst_id, config));
    return;
  }

  if (sscanf(cmd, "TearDownDataSourceInstance %" PRIu64, &inst_id) == 1) {
    // TODO what happens if Producer* goes away in the meantime?
    task_runner_->PostTask(
        std::bind(&Producer::TearDownDataSourceInstance, producer_, inst_id));
    return;
  }

  DLOG("Received unknown RPC from service: \"%s\"\n", cmd);
  DCHECK(false);
}

void UnixServiceProxyForProducer::RegisterDataSource(
    const DataSourceDescriptor& desc,
    RegisterDataSourceCallback callback) {
  pending_register_data_source_callback_ = callback;
  conn_.Send("RegisterDataSource " + desc.name);
}

void UnixServiceProxyForProducer::UnregisterDataSource(DataSourceID) {
  DCHECK(false);  // Not implemented.
}

SharedMemory* UnixServiceProxyForProducer::GetSharedMemory() {
  return shmem_.get();
}

void UnixServiceProxyForProducer::NotifyPageAcquired(uint32_t page_index) {
  conn_.Send("NotifyPageAcquired " + std::to_string(page_index));
}

void UnixServiceProxyForProducer::NotifyPageReleased(uint32_t page_index) {
  conn_.Send("NotifyPageReleased " + std::to_string(page_index));
}

}  // namespace perfetto
