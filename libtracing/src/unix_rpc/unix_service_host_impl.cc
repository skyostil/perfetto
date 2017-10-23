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

#include "libtracing/src/unix_rpc/unix_service_host_impl.h"

#include "libtracing/core/service.h"
#include "libtracing/core/task_runner.h"
#include "libtracing/src/core/base.h"
#include "libtracing/src/unix_rpc/unix_producer_proxy.h"
#include "libtracing/src/unix_rpc/unix_shared_memory.h"

namespace perfetto {

// Implements the publicly exposed factory method declared in
// include/libtracing/unix_rpc/unix_service_host.h.

// static
std::unique_ptr<UnixServiceHost> UnixServiceHost::CreateInstance(
    const char* socket_name,
    TaskRunner* task_runner,
    ObserverForTesting* observer_for_testing) {
  return std::unique_ptr<UnixServiceHost>(
      new UnixServiceHostImpl(socket_name, task_runner, observer_for_testing));
}

UnixServiceHostImpl::UnixServiceHostImpl(const char* socket_name,
                                         TaskRunner* task_runner,
                                         ObserverForTesting* observer)
    : socket_name_(socket_name),
      task_runner_(task_runner),
      observer_(observer) {
  std::unique_ptr<SharedMemory::Factory> shm_factory(
      new UnixSharedMemory::Factory());
  svc_ = Service::CreateInstance(std::move(shm_factory), task_runner);
}

UnixServiceHostImpl::~UnixServiceHostImpl() {}

bool UnixServiceHostImpl::Start() {
  if (!producer_port_.Listen(socket_name_))
    return false;
  auto callback = std::bind(&UnixServiceHostImpl::OnNewConnection, this);
  task_runner_->AddFileDescriptorWatch(producer_port_.fd(), callback);
  return true;
}

void UnixServiceHostImpl::OnNewConnection() {
  DLOG("[UnixServiceHostImpl] Woken up for new connection\n");
  UnixSocket client_connection;

  // TODO this shoould be a while() loop in nonblock mode or will miss
  // connections if two of them happen back to back with just one select()
  // notification.
  producer_port_.Accept(&client_connection);

  DLOG("[UnixServiceHostImpl] New connection established\n");
  UnixProducerProxy* unix_producer_proxy = new UnixProducerProxy(
      std::move(client_connection), task_runner_, observer_);

  Service::ProducerEndpoint* svc_endpoint = svc_->ConnectProducer(
      std::unique_ptr<UnixProducerProxy>(unix_producer_proxy));

  unix_producer_proxy->set_service(svc_endpoint);
  task_runner_->PostTask(std::bind(&ObserverForTesting::OnProducerConnected,
                                   observer_, svc_endpoint->GetID()));
}

Service* UnixServiceHostImpl::service_for_testing() const {
  return svc_.get();
}

}  // namespace perfetto
