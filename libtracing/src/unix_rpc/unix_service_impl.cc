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

#include "libtracing/src/unix_rpc/unix_service_impl.h"

#include <inttypes.h>
#include <stdio.h>

#include "libtracing/core/data_source_config.h"
#include "libtracing/core/data_source_descriptor.h"
#include "libtracing/core/producer.h"
#include "libtracing/core/service.h"
#include "libtracing/core/task_runner_proxy.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/unix_rpc/unix_shared_memory.h"

namespace perfetto {

////////////////////////////////////////////////////////////////////////////////
// Definition of the ProducerRPC class.
// This implements the core Producer interface and proxies requests and replies
// over a UnixSocket via a home-brewed poor man's RPC.
////////////////////////////////////////////////////////////////////////////////

class ProducerRPC : public Producer {
 public:
  ProducerRPC(ServiceImpl*, UnixSocket, UnixService::Delegate*);
  ~ProducerRPC() override;

  void SendSharedMemory(const UnixSharedMemory&);
  void OnDataAvailable();

  void set_id(ProducerID id) {
    DCHECK(id > 0);
    id_ = id;
  }

  UnixSocket* conn() { return &conn_; }

  // Producer implementation.
  void CreateDataSourceInstance(const DataSourceConfig& config,
                                DataSourceInstanceID instance_id) override;
  void TearDownDataSourceInstance(DataSourceInstanceID) override;
  void OnConnect() override;

 private:
  ProducerID id_ = 0;
  ServiceImpl* const svc_;
  UnixSocket conn_;
  UnixService::Delegate* const delegate_;
};

ProducerRPC::ProducerRPC(ServiceImpl* svc,
                         UnixSocket conn,
                         UnixService::Delegate* delegate)
    : svc_(svc), conn_(std::move(conn)), delegate_(delegate) {
  DCHECK(conn_.is_connected());
  delegate_->task_runner()->AddFileDescriptorWatch(
      conn_.fd(), std::bind(&ProducerRPC::OnDataAvailable, this));
}

ProducerRPC::~ProducerRPC() {}

void ProducerRPC::OnDataAvailable() {
  if (id_ == 0) {
    // We still haven't received the ConnectProducerCallback at this point so
    // don't have the ID for the peer yet, but we have already received data.
    // Just repost this task, shouldn't take that long really.
    delegate_->task_runner()->PostTask(
        std::bind(&ProducerRPC::OnDataAvailable, this));
    return;
  }

  char cmd[1024];
  ssize_t rsize = conn_.Recv(cmd, sizeof(cmd) - 1);
  if (rsize <= 0) {
    delegate_->task_runner()->RemoveFileDescriptorWatch(conn_.fd());
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
    auto callback = [this](DataSourceID dsid) {
      // TODO lifetime. Are we guaranteed that this object is not destroyed
      // while the callback is pending?
      // TODO what to do if the send fails (conn closed?) here and below?
      conn_.Send("RegisterDataSourceCallback " + std::to_string(dsid));
      delegate_->task_runner()->PostTask(std::bind(
          &UnixService::Delegate::OnDataSourceConnected, delegate_, dsid));
    };
    svc_->RegisterDataSource(id_, descriptor, callback);
    return;
  }

  uint32_t page_index;
  if (sscanf(cmd, "NotifyPageTaken %" PRIu32, &page_index) == 1) {
    svc_->NotifyPageTaken(id_, page_index);
    return;
  }

  if (sscanf(cmd, "NotifyPageReleased %" PRIu32, &page_index) == 1) {
    svc_->NotifyPageReleased(id_, page_index);
    return;
  }

  DLOG("Received unknown RPC from producer: \"%s\"\n", cmd);
  DCHECK(false);
}

void ProducerRPC::SendSharedMemory(const UnixSharedMemory& shm) {
  static const char kMsg[] = "SendSharedMemory";
  int fd = shm.fd();
  bool res = conn_.Send(kMsg, sizeof(kMsg), &fd, 1);
  DCHECK(res);
}

void ProducerRPC::CreateDataSourceInstance(const DataSourceConfig& config,
                                           DataSourceInstanceID instance_id) {
  conn_.Send("CreateDataSourceInstance " + std::to_string(instance_id) + " " +
             config.data_source_name + " " + config.trace_category_filters);
}

void ProducerRPC::TearDownDataSourceInstance(DataSourceInstanceID instance_id) {
  conn_.Send("TearDownDataSourceInstance " + std::to_string(instance_id));
}

// OnConnect is meant to be called ony on the libtracing client side, not from
// within th service business logic.
// TODO: smells like bad design.
void ProducerRPC::OnConnect() {
  DCHECK(false);
}

////////////////////////////////////////////////////////////////////////////////
// Definition of the UnixServiceImpl methods.
////////////////////////////////////////////////////////////////////////////////

// static
std::unique_ptr<UnixService> UnixService::CreateInstance(
    const char* socket_name,
    UnixService::Delegate* delegate) {
  return std::unique_ptr<UnixService>(
      new UnixServiceImpl(socket_name, delegate));
}

UnixServiceImpl::UnixServiceImpl(const char* socket_name,
                                 UnixService::Delegate* delegate)
    : socket_name_(socket_name), svc_(this), delegate_(delegate) {}

bool UnixServiceImpl::Start() {
  if (!producer_port_.Listen(socket_name_))
    return false;
  auto callback = [this]() {
    DLOG("[UnixServiceImpl] Woken up for new connection\n");
    UnixSocket client_connection;
    producer_port_.Accept(&client_connection);
    OnNewConnection(std::move(client_connection));
  };
  delegate_->task_runner()->AddFileDescriptorWatch(producer_port_.fd(),
                                                   callback);
  return true;
}

void UnixServiceImpl::CreateDataSourceInstanceForTesting(
    const DataSourceConfig& config,
    DataSourceInstanceID dsid) {
  svc_.CreateDataSourceInstanceForTesting(config, dsid);
}

void UnixServiceImpl::OnNewConnection(UnixSocket conn) {
  DLOG("[UnixServiceImpl] New connection established\n");
  std::unique_ptr<Producer> new_producer(
      new ProducerRPC(&svc_, std::move(conn), delegate_));
  auto callback = [](Producer* producer, ProducerID producer_id) {
    static_cast<ProducerRPC*>(producer)->set_id(producer_id);
  };
  svc_.ConnectProducer(std::move(new_producer), callback);
}

TaskRunnerProxy* UnixServiceImpl::task_runner() const {
  return delegate_->task_runner();
}

std::unique_ptr<SharedMemory> UnixServiceImpl::CreateSharedMemoryWithPeer(
    Producer* peer,
    size_t shm_size) {
  std::unique_ptr<UnixSharedMemory> shm = UnixSharedMemory::Create(shm_size);
  static_cast<ProducerRPC*>(peer)->SendSharedMemory(*shm);
  return std::move(shm);
}

}  // namespace perfetto
