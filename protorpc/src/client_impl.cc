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
#include "protorpc/method_invocation_reply.h"
#include "protorpc/service_proxy.h"

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

void ClientImpl::BindServiceGeneric(
    const std::string& service_name,
    std::function<void(std::shared_ptr<ServiceProxy>)> callback) {
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::BindService* req = rpc_frame.mutable_msg_bind_service();
  req->set_service_name(service_name);
  if (!SendRPCFrame(rpc_frame)) {
    DLOG("Failed BindService(%s)", service_name.c_str());
    task_runner_->PostTask(std::bind(callback, nullptr));
    return;
  }
  QueuedRequest qr;
  qr.type = RPCFrame::kMsgBindService;
  qr.request_id = request_id;
  qr.service_name = service_name;
  qr.bind_callback = std::move(callback);
  queued_requests_.emplace(request_id, std::move(qr));
}

RequestID ClientImpl::BeginInvoke(
    ServiceID service_id,
    MethodID method_id,
    ProtoMessage* method_args,
    const std::weak_ptr<ServiceProxy>& service_proxy) {
  std::string args_proto;
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::InvokeMethod* req = rpc_frame.mutable_msg_invoke_method();
  req->set_service_id(service_id);
  req->set_method_id(method_id);
  bool did_serialize = method_args->SerializeToString(&args_proto);
  req->set_args_proto(args_proto);
  if (!did_serialize || !SendRPCFrame(rpc_frame)) {
    return 0;
  }
  QueuedRequest qr;
  qr.type = RPCFrame::kMsgInvokeMethod;
  qr.method_id = method_id;
  qr.service_proxy = service_proxy;
  queued_requests_.emplace(request_id, std::move(qr));
  return request_id;
}

bool ClientImpl::SendRPCFrame(const RPCFrame& rpc_frame) {
  uint32_t payload_len = static_cast<uint32_t>(rpc_frame.ByteSize());
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (!rpc_frame.SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
    DCHECK(false);
    return false;
  }
  uint32_t enc_size = BYTE_SWAP_TO_LE32(payload_len);
  memcpy(buf.get(), &enc_size, sizeof(uint32_t));

  // TODO(primiano): remember that this is doing non-blocking I/O. What if the
  // socket buffer is full? Maybe we just want to drop this on the floor? Or
  // maybe throttle the send and PostTask the reply later?
  if (!sock_.Send(buf.get(), sizeof(uint32_t) + payload_len))
    return false;

  return true;
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
    OnRPCFrameReceived(*rpc_frame);
  }
}

void ClientImpl::OnRPCFrameReceived(const RPCFrame& rpc_frame) {
  auto queued_requests_it = queued_requests_.find(rpc_frame.request_id());
  if (queued_requests_it == queued_requests_.end()) {
    DLOG("OnRPCFrameReceived() unknown req %" PRIu64, rpc_frame.request_id());
    return;
  }
  QueuedRequest req = std::move(queued_requests_it->second);
  queued_requests_.erase(queued_requests_it);
  req.succeeded = rpc_frame.reply_success();

  if (req.type != rpc_frame.msg_case()) {
    DLOG(
        "The server is drunk. We requestes msg_type=%d but received "
        "msg_type=%d in reply for request_id=%" PRIu64,
        req.type, rpc_frame.msg_case(), rpc_frame.request_id());
    req.succeeded = false;
  }

  switch (req.type) {
    case RPCFrame::kMsgBindServiceReply:
      OnBindServiceReply(std::move(req), rpc_frame.msg_bind_service_reply());
      break;
    case RPCFrame::kMsgInvokeMethodReply: {
      OnInvokeMethodReply(std::move(req), rpc_frame.msg_invoke_method_reply());
      break;
      // These messages are only valid in the Client -> Host direction.
      case RPCFrame::kMsgBindService:
      case RPCFrame::kMsgInvokeMethod:
      case RPCFrame::MSG_NOT_SET:
        DLOG("Received invalid RPC frame %u", rpc_frame.msg_case());
        return;
    }
  }
}

void ClientImpl::OnBindServiceReply(QueuedRequest req,
                                    const RPCFrame::BindServiceReply& reply) {
  if (req.succeeded) {
    DLOG("Failed to bind Service \"%s\"", req.service_name.c_str());
    task_runner_->PostTask(std::bind(req.bind_callback, nullptr));
    return;
  }
  std::map<std::string, MethodID> methods;
  for (const auto& method : reply.methods()) {
    if (method.name().empty() || method.id() <= 0) {
      DLOG("OnBindServiceReply() invalid method \"%s\" -> %" PRIu32,
           method.name().c_str(), method.id());
      continue;
    }
    methods[method.name()] = method.id();
  }
  std::shared_ptr<ServiceProxy> service_proxy(
      new ServiceProxy(this, reply.service_id(), std::move(methods)));
  DCHECK(req.bind_callback);
  task_runner_->PostTask(
      std::bind(req.bind_callback, std::move(service_proxy)));
}

void ClientImpl::OnInvokeMethodReply(QueuedRequest req,
                                     const RPCFrame::InvokeMethodReply& reply) {
  DCHECK(req.invoke_callback);
  bool success = req.succeeded;
  bool decoded = reply.reply_proto();
  success = decoded ? success : false;
  MethodInvocationReplyBase reply_arg(success, reply.eof(), reply_proto);
  task_runner_->PostTask(
      EndInvokeClosure(req.service_proxy, req.request_id, success));
}

ClientImpl::ServiceBinding::ServiceBinding() = default;
ClientImpl::ServiceBinding::Method::Method() = default;
ClientImpl::QueuedRequest::QueuedRequest() = default;

}  // namespace protorpc
}  // namespace perfetto
