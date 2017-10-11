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

#ifndef LIBTRACING_SRC_UNIX_RPC_UNIX_SERVICE_IMPL_H_
#define LIBTRACING_SRC_UNIX_RPC_UNIX_SERVICE_IMPL_H_

#include <memory>

#include "libtracing/core/basic_types.h"
#include "libtracing/core/service.h"
#include "libtracing/src/core/service_impl.h"
#include "libtracing/src/unix_rpc/unix_socket.h"
#include "libtracing/unix_rpc/unix_service.h"

namespace perfetto {

class UnixSocket;

class UnixServiceImpl : public UnixService, public ServiceImpl::Delegate {
 public:
  UnixServiceImpl(const char* socket_name, UnixService::Delegate*);
  ~UnixServiceImpl() override {}

  // UnixService implementation.
  bool Start() override;
  void CreateDataSourceInstanceForTesting(const DataSourceConfig&,
                                          DataSourceInstanceID) override;

  // ServiceImpl::Delegate implementation.
  TaskRunnerProxy* task_runner() const override;
  std::unique_ptr<SharedMemory> CreateSharedMemoryWithPeer(
      Producer* peer,
      size_t shm_size) override;

 private:
  void OnNewConnection(UnixSocket conn);

  const char* const socket_name_;
  UnixSocket producer_port_;  // The listening sock where Producers connect to.
  ServiceImpl svc_;
  UnixService::Delegate* delegate_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_UNIX_RPC_UNIX_SERVICE_IMPL_H_
