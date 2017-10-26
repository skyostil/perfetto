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

#ifndef PROTORPC_SRC_CLIENT_IMPL_H_
#define PROTORPC_SRC_CLIENT_IMPL_H_

#include "cpp_common/task_runner.h"
#include "protorpc/client.h"
#include "protorpc/src/rpc_frame_decoder.h"
#include "protorpc/src/unix_socket.h"

#include <map>
#include <memory>
#include <vector>

namespace perfetto {
namespace protorpc {

class RPCFrame;
class ServiceDescriptor;

class ClientImpl : public Client {
 public:
  ClientImpl(const char* socket_name, TaskRunner*);
  ~ClientImpl() override;

  bool Connect();

  // Client implementation.
  void BindServiceGeneric(
      const std::string& service_name,
      std::function<void(std::shared_ptr<ServiceProxy>)>) override;

  RequestID BeginInvoke(ServiceID,
                        MethodID,
                        ProtoMessage*,
                        const std::weak_ptr<ServiceProxy>&) override;

 private:
  struct QueuedRequest {
    QueuedRequest();
    int type = 0;  // From RPCFrame::msg_case() (see wire_protocol.proto).
    RequestID request_id = 0;
    bool succeeded = false;

    // only for type == kMsgBindService.
    std::string service_name;
    std::function<void(std::shared_ptr<ServiceProxy>)> bind_callback;

    // only for type == kMsgInvokeMethod.
    MethodID method_id;
    std::weak_ptr<ServiceProxy> service_proxy;
  };
  ClientImpl(const ClientImpl&) = delete;
  ClientImpl& operator=(const ClientImpl&) = delete;

  bool SendRPCFrame(const RPCFrame&);
  void OnDataAvailable();
  void OnRPCFrameReceived(const RPCFrame&);
  void OnBindServiceReply(QueuedRequest, const RPCFrame::BindServiceReply&);
  void OnInvokeMethodReply(QueuedRequest, const RPCFrame::InvokeMethodReply&);

  const char* const socket_name_;
  TaskRunner* const task_runner_;
  UnixSocket sock_;
  RequestID last_request_id_ = 0;
  RPCFrameDecoder frame_decoder;
  std::map<RequestID, QueuedRequest> queued_requests_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_CLIENT_IMPL_H_
