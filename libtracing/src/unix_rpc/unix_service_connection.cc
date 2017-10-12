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

#include "libtracing/unix_rpc/unix_service_connection.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/producer.h"
#include "libtracing/core/service.h"
#include "libtracing/core/task_runner_proxy.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/unix_rpc/unix_shared_memory.h"
#include "libtracing/src/unix_rpc/unix_socket.h"

// TODO think to what happens when ServiceRPC gets destroyed w.r.t. the Producer
// pointer. Also think to lifetime of the Producer* during the callbacks.

namespace perfetto {

////////////////////////////////////////////////////////////////////////////////
// This implements the core Service interface and proxies requests and replies
// to a UnixServiceImpl instance via a home-brewed poor man's RPC (brutal
// printf/scanf over a UNIX socket).
////////////////////////////////////////////////////////////////////////////////

class ServiceRPC : public Service {
 public:
  explicit ServiceRPC(Producer* producer, TaskRunnerProxy* task_runner)
      : producer_(producer), task_runner_(task_runner) {}

  bool Connect(const char* service_socket_name);

  void ConnectProducer(std::unique_ptr<Producer>,
                       ConnectProducerCallback) override;
  void RegisterDataSource(ProducerID,
                          const DataSourceDescriptor&,
                          RegisterDataSourceCallback) override;
  void UnregisterDataSource(DataSourceID) override;
  void NotifyPageTaken(ProducerID, uint32_t page_index) override;
  void NotifyPageReleased(ProducerID, uint32_t page_index) override;
  SharedMemory* GetSharedMemoryForProducer(ProducerID) override;

 private:
  void OnDataAvailable();

  Producer* const producer_;
  TaskRunnerProxy* task_runner_;
  RegisterDataSourceCallback pending_register_data_source_callback_;
  UnixSocket conn_;
  std::unique_ptr<UnixSharedMemory> shmem_;
};

bool ServiceRPC::Connect(const char* service_socket_name) {
  if (!conn_.Connect(service_socket_name))
    return false;
  task_runner_->AddFileDescriptorWatch(
      conn_.fd(), std::bind(&ServiceRPC::OnDataAvailable, this));
  return true;
}

void ServiceRPC::OnDataAvailable() {
  DLOG("[unix_service_connection.cc] ServiceRPC::OnDataAvailable\n");
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
    DLOG("[unix_service_connection.cc] Received shm, fd=%d\n", shm_fd);
    shmem_ = UnixSharedMemory::CreateFromFD(shm_fd);
    DCHECK(shmem_);
    DLOG("[unix_service_connection.cc] Mapped shm, size=%lu\n", shmem_->size());
    producer_->OnConnect();  // TODO PostTask?
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
    producer_->CreateDataSourceInstance(config, inst_id);  // TODO PostTask?
    return;
  }

  if (sscanf(cmd, "TearDownDataSourceInstance %" PRIu64, &inst_id) == 1) {
    producer_->TearDownDataSourceInstance(inst_id);  // TODO PostTask?
    return;
  }

  DLOG("Received unknown RPC from service: \"%s\"\n", cmd);
  DCHECK(false);
}

void ServiceRPC::ConnectProducer(std::unique_ptr<Producer>,
                                 ConnectProducerCallback) {}

void ServiceRPC::RegisterDataSource(ProducerID,
                                    const DataSourceDescriptor& desc,
                                    RegisterDataSourceCallback callback) {
  pending_register_data_source_callback_ = callback;
  conn_.Send("RegisterDataSource " + desc.name);
}

void ServiceRPC::UnregisterDataSource(DataSourceID) {
  // I am too lazy for this. Respectable data sources never give up :P
  DCHECK(false);
}

// TODO: Hmm smells like the ProducerID argument isn't really needed below.
// I mean, this couldn't be anything other then our own ID.

void ServiceRPC::NotifyPageTaken(ProducerID, uint32_t page_index) {
  conn_.Send("NotifyPageTaken " + std::to_string(page_index));
}

void ServiceRPC::NotifyPageReleased(ProducerID, uint32_t page_index) {
  conn_.Send("NotifyPageReleased " + std::to_string(page_index));
}

SharedMemory* ServiceRPC::GetSharedMemoryForProducer(ProducerID) {
  return shmem_.get();
}

////////////////////////////////////////////////////////////////////////////////
// Definition of the UnixServiceConnection methods.
////////////////////////////////////////////////////////////////////////////////

// static
std::unique_ptr<Service> UnixServiceConnection::ConnectAsProducer(
    const char* service_socket_name,
    Producer* producer,
    TaskRunnerProxy* task_runner) {
  std::unique_ptr<ServiceRPC> service_rpc(
      new ServiceRPC(producer, task_runner));

  // TODO set short timeout here. maybe make async?
  if (!service_rpc->Connect(service_socket_name))
    return nullptr;
  return std::move(service_rpc);
}

}  // namespace perfetto
