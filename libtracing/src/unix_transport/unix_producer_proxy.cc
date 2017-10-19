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

#include "libtracing/src/unix_transport/unix_producer_proxy.h"

#include <inttypes.h>
#include <stdio.h>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/service.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/unix_transport/unix_shared_memory.h"

namespace perfetto {

UnixProducerProxy::UnixProducerProxy(
    Service* svc,
    UnixSocket conn,
    TaskRunner* task_runner,
    UnixServiceHost::ObserverForTesting* observer)
    : svc_(svc),
      conn_(std::move(conn)),
      task_runner_(task_runner),
      observer_(observer) {
  DCHECK(conn_.is_connected());
  task_runner_->AddFileDescriptorWatch(
      conn_.fd(), std::bind(&UnixProducerProxy::OnDataAvailable, this));
}

UnixProducerProxy::~UnixProducerProxy() {}

void UnixProducerProxy::OnDataAvailable() {
  if (id_ == 0) {
    // We still haven't received the ConnectProducerCallback at this point so
    // don't have the ID for the peer yet, but we have already received data.
    // Just repost this task, shouldn't take that long really.
    task_runner_->PostTask(
        std::bind(&UnixProducerProxy::OnDataAvailable, this));
    return;
  }

  char cmd[1024];
  ssize_t rsize = conn_.Recv(cmd, sizeof(cmd) - 1);
  if (rsize <= 0) {
    task_runner_->RemoveFileDescriptorWatch(conn_.fd());
    return;  // TODO connection closed (very likely peer died. do something.
  }

  cmd[rsize] = '\0';

  // TODO if we home brew the RPC protocol, each message should be prefixed by
  // a counter (think to IMAP protocol), so that we can match requests and
  // replies. For instance, if we get two RegisterDataSource requests back to
  // back, how does the client know which reply is which? Either we just rely
  // on FIFO (maybe okay given this is a SOCK_STREAM socket) or we do something
  // like:
  //     -> 10 RegisterDataSource Foo
  //     -> 20 RegisterDataSource Bar
  //     <- 10 RegisterDataSourceReply FooID
  //     <- 20 RegisterDataSourceReply BarID
  // For the moment not going to bother and just assuming FIFO ordering.

  // Also all these payloads here should really be protobuf. But for the sake of
  // sketching this is going to be just a poor man's RPC.

  char data_source_name[64];
  if (sscanf(cmd, "RegisterDataSource %64s", data_source_name) == 1) {
    DataSourceDescriptor descriptor{std::string(data_source_name)};
    DataSourceID dsid = svc_->RegisterDataSource(id_, descriptor);

    // TODO what to do if the send fails (conn closed?) here and below?
    conn_.Send("RegisterDataSourceCallback " + std::to_string(dsid));
    task_runner_->PostTask(
        std::bind(&UnixServiceHost::ObserverForTesting::OnDataSourceRegistered,
                  observer_, dsid));
    return;
  }

  uint32_t page_index;
  if (sscanf(cmd, "NotifyPageAcquired %" PRIu32, &page_index) == 1) {
    svc_->NotifyPageAcquired(id_, page_index);
    return;
  }

  if (sscanf(cmd, "NotifyPageReleased %" PRIu32, &page_index) == 1) {
    svc_->NotifyPageReleased(id_, page_index);
    return;
  }

  DLOG("Received unknown RPC from producer: \"%s\"\n", cmd);
  DCHECK(false);
}

void UnixProducerProxy::SetId(ProducerID id) {
  DCHECK(id > 0);
  id_ = id;
}

std::unique_ptr<SharedMemory> UnixProducerProxy::InitializeSharedMemory(
    size_t shm_size) {
  std::unique_ptr<UnixSharedMemory> shm = UnixSharedMemory::Create(shm_size);
  if (!shm)
    return nullptr;
  static const char kMsg[] = "SendSharedMemory";
  int fd = shm->fd();
  bool res = conn_.Send(kMsg, sizeof(kMsg), &fd, 1);
  DCHECK(res);
  return std::move(shm);
}

void UnixProducerProxy::CreateDataSourceInstance(
    DataSourceInstanceID instance_id,
    const DataSourceConfig& config) {
  conn_.Send("CreateDataSourceInstance " + std::to_string(instance_id) + " " +
             config.data_source_name + " " + config.trace_category_filters);
}

void UnixProducerProxy::TearDownDataSourceInstance(
    DataSourceInstanceID instance_id) {
  conn_.Send("TearDownDataSourceInstance " + std::to_string(instance_id));
}

}  // namespace perfetto
