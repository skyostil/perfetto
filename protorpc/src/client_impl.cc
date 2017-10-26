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

#include <inttypes.h>

#include "cpp_common/task_runner.h"
#include "protorpc/host_handle.h"
#include "protorpc/service_stub.h"

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

ClientImpl::~ClientImpl() {
  if (sock_.fd() >= 0)
    task_runner_->RemoveFileDescriptorWatch(sock_.fd());
}

bool ClientImpl::Connect() {
  // TODO does Connect() work synchronously fine in non-blocking mode for a
  // unix socket? Maybe this should also use a watch.
  sock_.SetBlockingIOMode(false);
  if (!sock_.Connect(socket_name_))
    return false;
  task_runner_->AddFileDescriptorWatch(
      sock_.fd(), std::bind(&ClientImpl::OnDataAvailable, this));
  return true;
}

void ClientImpl::BindService(const std::string& service_name,
                             BindServiceCallback callback) {
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::BindService* req = rpc_frame.mutable_msg_bind_service();
  req->set_service_name(service_name);

  uint32_t payload_len = static_cast<uint32_t>(rpc_frame.ByteSize());
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (!rpc_frame.SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
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
  service_bindings_.emplace(service_name, ServiceBinding());
  QueuedRequest qr;
  qr.type = RPCFrame::kMsgBindService;
  qr.service_name = service_name;
  queued_request_.emplace(request_id, std::move(qr));
}

void ClientImpl::OnDataAvailable() {
  ssize_t rsize;
  do {
    std::pair<char*, size_t> buf = frame_decoder.GetRecvBuffer();
    rsize = sock_.Recv(buf.first, buf.second);
    frame_decoder.SetLastReadSize(rsize);
    // TODO(primiano): Recv should return some different code to distinguish
    // EWOULDBLOCK from a generic error.
  } while (rsize > 0);

  for (;;) {
    std::unique_ptr<RPCFrame> rpc_frame = frame_decoder.GetRPCFrame();
    if (!rpc_frame)
      break;

    auto queued_request_it = queued_request_.find(rpc_frame->request_id());
    if (queued_request_it == queued_request_.end()) {
      DLOG("Invalid RPC, unknown req id %" PRIu64, rpc_frame->request_id());
      continue;
    }
    QueuedRequest req = std::move(queued_request_it->second);

    if (req.type != rpc_frame->msg_case()) {
      // TODO if the reply id is not a kMsgBindServiceReply (the server would
      // gbe utterly broken in this case, but still) we should NACK the pending
      // callback.
    }

    switch (rpc_frame->msg_case()) {
      case RPCFrame::kMsgBindServiceReply: {
        const auto& reply = rpc_frame->msg_bind_service_reply();
        auto binding_it = service_bindings_.find(req.service_name);
        if (binding_it == service_bindings_.end()) {
          DLOG("No binding requested for service %s", req.service_name.c_str());
          continue;
        }
        ServiceBinding& binding = binding_it->second;
        binding.valid = true;
        binding.service_id = reply.service_id();
        for (const auto& method : reply.methods()) {
          ServiceBinding::Method& m = methods.emplace_back({});
          if (!method.has_name() || method.name().empty() || method.id() <= 0) {
            DLOG("Received invalid method \"%s\" -> %" PRIu32, method.name(),
                 method.id());
             continue;
          }
          m.name = method.name();
          m.id = method.id();
        }
        
        break;
      }
      case RPCFrame::kMsgInvokeMethodReply: {
        break;
      }
      // These messages are only valid in the Client -> Host direction.
      case RPCFrame::kMsgBindService:
      case RPCFrame::kMsgInvokeMethod:
      case RPCFrame::MSG_NOT_SET:
        DLOG("Received invalid RPC frame %u", rpc_frame->msg_case());
        return;
    }
  }
}

ClientImpl::ServiceBinding::ServiceBinding() = default;
ClientImpl::ServiceBinding::Method::Method() = default;
ClientImpl::QueuedRequest::QueuedRequest() = default;

}  // namespace protorpc
}  // namespace perfetto
