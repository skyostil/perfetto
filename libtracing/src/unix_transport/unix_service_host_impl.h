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

#ifndef LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_HOST_IMPL_H_
#define LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_HOST_IMPL_H_

#include <memory>

#include "libtracing/src/unix_transport/unix_socket.h"
#include "libtracing/unix_transport/unix_service_host.h"

namespace perfetto {

class Service;

class UnixServiceHostImpl : public UnixServiceHost {
 public:
  UnixServiceHostImpl(const char* socket_name,
                      TaskRunner*,
                      ObserverForTesting* = nullptr);
  ~UnixServiceHostImpl() override;

  // UnixServiceHost implementation.
  bool Start() override;
  Service* service_for_testing() const override;

 private:
  UnixServiceHostImpl(const UnixServiceHostImpl&) = delete;
  UnixServiceHostImpl& operator=(const UnixServiceHostImpl&) = delete;
  void OnNewConnection();

  const char* const socket_name_;
  TaskRunner* const task_runner_;
  ObserverForTesting* const observer_;
  UnixSocket producer_port_;
  std::unique_ptr<Service> svc_;
};

}  // namespace perfetto

#endif  // LIBTRACING_SRC_UNIX_TRANSPORT_UNIX_SERVICE_HOST_IMPL_H_
