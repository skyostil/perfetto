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

// TODO(primiano): protobuf leaks CHECK macro, sigh.
#include "wire_protocol.pb.h"

#include "protorpc/src/client_impl.h"

#include "cpp_common/task_runner.h"
#include "protorpc/host_handle.h"

// TODO(primiano): Add ThreadChecker everywhere.

namespace perfetto {
namespace protorpc {

// static
std::unique_ptr<Client> Client::CreateInstance(const char* socket_name,
                                               TaskRunner* task_runner) {
  std::unique_ptr<ClientImpl> client(new ClientImpl(socket_name, task_runner));
  if (!client->Connect())
    return nullptr;
  return std::unique_ptr<Client>(client.release());
}

ClientImpl::ClientImpl(const char* socket_name, TaskRunner* task_runner)
    : socket_name_(socket_name), task_runner_(task_runner) {}

ClientImpl::~ClientImpl() = default;

bool ClientImpl::Connect() {
  if (!sock_.Connect(socket_name_))
    return false;
  task_runner_->AddFileDescriptorWatch(sock_.fd(),
                                       std::bind(&ClientImpl::OnDataAvailable));
  return true;
}

void ClientImpl::BindService(const std::string& service_name,
                             BindServiceCallback callback) {
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::BindService* req = rpc_frame.mutable_msg_bind_service();
  req->set_service_name(service_name);

  uint32_t payload_len = static_cast<uint32_t>(rpc_frame->ByteSize());
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (!rpc_frame->SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
    DCHECK(false);
    task_runner_->PostTask(std::bind(callback, nullptr));
    return;
  }
  uint32_t enc_size = BYTE_SWAP_TO_LE32(payload_len);
  memcpy(buf.get(), &enc_size, sizeof(uint32_t));

  // TODO(primiano): remember that this is doing non-blocking I/O. What if the
  // socket buffer is full? Maybe we just want to drop this on the floor? Or
  // maybe throttle the send and PostTask the reply later?
  if (!sock_.Send(buf.get(), sizeof(uint32_t) + payload_len)) {
    task_runner_->PostTask(std::bind(callback, nullptr));
    return;
  }

  queued_request_[request_id] = ...;
}

void OnDataAvailable() {}

}  // namespace protorpc
}  // namespace perfetto
