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

// TODO(primiano): protobuf leaks PERFETTO_CHECK macro, sigh. switch us to
// PERFETTO_CHECK and put this header back down.
#include "wire_protocol.pb.h"

#include "protorpc/src/client_impl.h"

#include <inttypes.h>

#include "base/task_runner.h"
#include "base/utils.h"
#include "protorpc/service_descriptor.h"
#include "protorpc/service_proxy.h"

// TODO(primiano): Add ThreadChecker everywhere.

namespace perfetto {
namespace protorpc {

// static
std::shared_ptr<Client> Client::CreateInstance(const char* socket_name,
                                               base::TaskRunner* task_runner) {
  std::shared_ptr<ClientImpl> client(new ClientImpl(socket_name, task_runner));
  client->set_weak_ptr(std::weak_ptr<Client>(client));
  if (!client->Connect())
    return nullptr;
  return client;
}

ClientImpl::ClientImpl(const char* socket_name, base::TaskRunner* task_runner)
    : socket_name_(socket_name), task_runner_(task_runner) {}

ClientImpl::~ClientImpl() {
  if (sock_.is_connected())  // Not 100% correct, what if got disconnected. but
                             // also can't just use the fd >= 0.
    task_runner_->RemoveFileDescriptorWatch(sock_.fd());
}

bool ClientImpl::Connect() {
  // TODO does Connect() work synchronously fine in non-blocking mode for a
  // unix socket? Maybe this should also use a watch.
  if (!sock_.Connect(socket_name_))
    return false;
  sock_.SetBlockingIOMode(false);
  task_runner_->AddFileDescriptorWatch(
      sock_.fd(), std::bind(&ClientImpl::OnDataAvailable, this));
  return true;
}

void ClientImpl::BindService(const std::weak_ptr<ServiceProxy>& weak_service) {
  std::shared_ptr<ServiceProxy> service_proxy = weak_service.lock();
  if (!service_proxy)
    return;
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::BindService* req = rpc_frame.mutable_msg_bind_service();
  const std::string& service_name = service_proxy->GetDescriptor().service_name;
  req->set_service_name(service_name);
  if (!SendRPCFrame(rpc_frame)) {
    PERFETTO_DLOG("BindService(%s) failed", service_name.c_str());
    service_proxy->event_listener()->OnConnectionFailed();
  }
  QueuedRequest qr;
  qr.type = RPCFrame::kMsgBindService;
  qr.request_id = request_id;
  qr.service_proxy = std::weak_ptr<ServiceProxy>(service_proxy);
  queued_requests_.emplace(request_id, std::move(qr));
}  // namespace protorpc

RequestID ClientImpl::BeginInvoke(
    ServiceID service_id,
    const std::string& method_name,
    MethodID remote_method_id,
    const ProtoMessage& method_args,
    const std::weak_ptr<ServiceProxy>& service_proxy) {
  std::string args_proto;
  RequestID request_id = ++last_request_id_;
  RPCFrame rpc_frame;
  rpc_frame.set_request_id(request_id);
  RPCFrame::InvokeMethod* req = rpc_frame.mutable_msg_invoke_method();
  req->set_service_id(service_id);
  req->set_method_id(remote_method_id);
  bool did_serialize = method_args.SerializeToString(&args_proto);
  req->set_args_proto(args_proto);
  if (!did_serialize || !SendRPCFrame(rpc_frame)) {
    return 0;
  }
  QueuedRequest qr;
  qr.type = RPCFrame::kMsgInvokeMethod;
  qr.request_id = request_id;
  qr.method_name = method_name;
  qr.service_proxy = service_proxy;
  queued_requests_.emplace(request_id, std::move(qr));
  return request_id;
}

bool ClientImpl::SendRPCFrame(const RPCFrame& rpc_frame) {
  uint32_t payload_len = static_cast<uint32_t>(rpc_frame.ByteSize());
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (!rpc_frame.SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
    PERFETTO_DCHECK(false);
    return false;
  }
  memcpy(buf.get(), ASSUME_LITTLE_ENDIAN(&payload_len), sizeof(uint32_t));

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
    PERFETTO_DLOG("OnRPCFrameReceived() unknown req %" PRIu64,
                  rpc_frame.request_id());
    return;
  }
  QueuedRequest req = std::move(queued_requests_it->second);
  queued_requests_.erase(queued_requests_it);
  req.succeeded = rpc_frame.reply_success();

  if (req.type == RPCFrame::kMsgBindService &&
      rpc_frame.msg_case() == RPCFrame::kMsgBindServiceReply) {
    return OnBindServiceReply(std::move(req),
                              rpc_frame.msg_bind_service_reply());
  }
  if (req.type == RPCFrame::kMsgInvokeMethod &&
      rpc_frame.msg_case() == RPCFrame::kMsgInvokeMethodReply) {
    return OnInvokeMethodReply(std::move(req),
                               rpc_frame.msg_invoke_method_reply());
  }

  PERFETTO_DLOG(
      "We requestes msg_type=%d but received msg_type=%d in reply to "
      "request_id=%" PRIu64,
      req.type, rpc_frame.msg_case(), rpc_frame.request_id());
}

void ClientImpl::OnBindServiceReply(QueuedRequest req,
                                    const RPCFrame::BindServiceReply& reply) {
  std::shared_ptr<ServiceProxy> service_proxy = req.service_proxy.lock();
  if (!service_proxy)
    return;
  if (!req.succeeded) {
    PERFETTO_DLOG("Failed BindService(%s)",
                  service_proxy->GetDescriptor().service_name.c_str());
    return service_proxy->event_listener()->OnConnectionFailed();
  }
  std::map<std::string, MethodID> methods;
  for (const auto& method : reply.methods()) {
    if (method.name().empty() || method.id() <= 0) {
      PERFETTO_DLOG("OnBindServiceReply() invalid method \"%s\" -> %" PRIu32,
                    method.name().c_str(), method.id());
      continue;
    }
    methods[method.name()] = method.id();
  }
  service_proxy->InitializeBinding(req.service_proxy, weak_ptr_self_,
                                   reply.service_id(), std::move(methods));
  service_proxy->event_listener()->OnConnect();
}

void ClientImpl::OnInvokeMethodReply(QueuedRequest req,
                                     const RPCFrame::InvokeMethodReply& reply) {
  std::shared_ptr<ServiceProxy> service_proxy = req.service_proxy.lock();
  if (!service_proxy)
    return;
  std::unique_ptr<ProtoMessage> decoded_reply;
  if (req.succeeded) {
    // TODO this could be optimized, stop doing method name string lookups.
    for (const auto& method : service_proxy->GetDescriptor().methods) {
      if (req.method_name == method.name) {
        decoded_reply = method.reply_proto_decoder(reply.reply_proto());
        break;
      }
    }
  }
  service_proxy->EndInvoke(req.request_id, std::move(decoded_reply),
                           reply.has_more());
}

ClientImpl::QueuedRequest::QueuedRequest() = default;

}  // namespace protorpc
}  // namespace perfetto
