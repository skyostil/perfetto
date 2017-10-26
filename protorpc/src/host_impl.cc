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

#include "protorpc/src/host_impl.h"

#include <inttypes.h>

#include <algorithm>
#include <utility>

#include "cpp_common/task_runner.h"
#include "protorpc/service.h"
#include "protorpc/service_descriptor.h"
#include "protorpc/service_reply.h"
#include "protorpc/service_request.h"

// TODO(primiano): Add ThreadChecker everywhere.

namespace perfetto {
namespace protorpc {

// static
std::shared_ptr<Host> Host::CreateInstance(const char* socket_name,
                                           TaskRunner* task_runner) {
  std::shared_ptr<HostImpl> host(new HostImpl(socket_name, task_runner));
  host->set_weak_ptr(std::weak_ptr<Host>(host));
  return host;
}

HostImpl::HostImpl(const char* socket_name, TaskRunner* task_runner)
    : socket_name_(socket_name), task_runner_(task_runner) {
  GOOGLE_PROTOBUF_VERIFY_VERSION;
}

HostImpl::~HostImpl() {
  if (sock_.fd() >= 0)
    task_runner_->RemoveFileDescriptorWatch(sock_.fd());
}

bool HostImpl::Start() {
  if (!sock_.Listen(socket_name_))
    return false;
  sock_.SetBlockingIOMode(false);
  // TODO use the weak ptr.
  task_runner_->AddFileDescriptorWatch(
      sock_.fd(), std::bind(&HostImpl::OnNewConnection, this));
  return true;
}

void HostImpl::OnNewConnection() {
  UnixSocket cli_sock;
  while (sock_.Accept(&cli_sock)) {
    int cli_sock_fd = cli_sock.fd();
    if (cli_sock_fd < 0) {
      DCHECK(false);
      continue;
    }
    // TODO(primiano): careful with Send() and non-blocking mode.
    cli_sock.SetBlockingIOMode(false);
    std::unique_ptr<ClientConnection> client(new ClientConnection());
    client->sock = std::move(cli_sock);
    ClientID client_id = ++last_client_id_;
    clients_[client_id] = std::move(client);
    // TODO use the weak ptr.
    task_runner_->AddFileDescriptorWatch(
        cli_sock_fd, std::bind(&HostImpl::OnDataAvailable, this, client_id));
  }
}

void HostImpl::OnDataAvailable(ClientID client_id) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;
  ClientConnection* client = client_it->second.get();
  RPCFrameDecoder& frame_decoder = client->frame_decoder;

  ssize_t rsize;
  do {
    std::pair<char*, size_t> buf = frame_decoder.GetRecvBuffer();
    rsize = client->sock.Recv(buf.first, buf.second);
    frame_decoder.SetLastReadSize(rsize);
    // TODO(primiano): Recv should return some different code to distinguish
    // EWOULDBLOCK from a generic error.
  } while (rsize > 0);

  for (;;) {
    std::unique_ptr<RPCFrame> rpc_frame = frame_decoder.GetRPCFrame();
    if (!rpc_frame)
      break;
    OnReceivedRPCFrame(client_id, client, *rpc_frame);
  }

  if (rsize == 0)
    return OnClientDisconnect(client_id);
}

void HostImpl::OnReceivedRPCFrame(ClientID client_id,
                                  ClientConnection* client,
                                  const RPCFrame& req_frame) {
  std::unique_ptr<RPCFrame> reply_frame(new RPCFrame());
  reply_frame->set_request_id(req_frame.request_id());
  reply_frame->set_reply_success(false);
  if (req_frame.msg_case() == RPCFrame::kMsgBindService) {
    return OnBindService(client, req_frame.msg_bind_service(),
                         std::move(reply_frame));

  } else if (req_frame.msg_case() == RPCFrame::kMsgBindService) {
    return OnInvokeMethod(client, req_frame.msg_invoke_method(),
                          std::move(reply_frame));
  }
  DLOG("Received invalid RPC frame %u from client %" PRIu64,
       req_frame.msg_case(), client_id);
  SendRPCFrame(client, std::move(reply_frame));
}

void HostImpl::OnBindService(ClientConnection* client,
                             const RPCFrame::BindService& req,
                             std::unique_ptr<RPCFrame> reply_frame) {
  auto* reply = reply_frame->mutable_msg_bind_service_reply();
  const ExposedService* service = GetServiceByName(req.service_name());
  if (service) {
    reply_frame->set_reply_success(true);
    reply->set_service_id(service->id);
    for (const auto& method_it : service->instance->GetDescriptor().methods) {
      RPCFrame::BindServiceReply::Method* method = reply->add_methods();
      method->set_name(method_it.second.name);
      method->set_id(method_it.first);
    }
  }
  SendRPCFrame(client, std::move(reply_frame));
}

void HostImpl::OnInvokeMethod(ClientConnection* client,
                              const RPCFrame::InvokeMethod& req,
                              std::unique_ptr<RPCFrame> reply_frame) {
  auto* reply = reply_frame->mutable_msg_invoke_method_reply();
  reply_frame->set_reply_success(true);
  auto svc_it = services_.find(req.service_id());
  if (svc_it == services_.end())
    break;

  const ServiceDescriptor& svc = svc_it->second.instance->GetDescriptor();
  const auto& methods = svc.methods;
  if (req.method_id() <= 0 || req.method_id() > methods.size())
    break;

  // TODO find here;
  // const ServiceDescriptor::Method& method = methods[req.method_id()];
  // std::unique_ptr<ProtoMessage> in_args =
  // method.decoder(req.args_proto()); if (!in_args)
  break;

  ((*svc.service).*(method.function))(
      ServiceRequestBase(std::move(in_args)),
      ServiceReplyBase(client_id, req_frame.request_id(), HostHandle(this),
                       method.new_reply_obj()));
}

void HostImpl::OnClientDisconnect(ClientID client_id) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end() || client_it->second->sock.fd() < 0) {
    DCHECK(false);
    return;
  }
  DLOG("[protorpc::HostImpl] Client %" PRIu64 " disconnected", client_id);
  task_runner_->RemoveFileDescriptorWatch(client_it->second->sock.fd());
  clients_.erase(client_id);
}

bool HostImpl::ExposeService(Service* service) {
  const ServiceDescriptor& desc = service->GetDescriptor();
  if (GetServiceByName(desc.service_name)) {
    DLOG("Duplicate ExposeService(): %s", desc.service_name.c_str());
    return;
  }
  ServiceID sid = ++last_service_id_;
  services_.emplace(sid, service);
}

const HostImpl::ExposedService* HostImpl::GetServiceByName(
    const std::string& name) {
  for (const auto& it : services_) {
    if (it.second.name == name)
      return &it.second;
  }
  return nullptr;
}

void HostImpl::ReplyToMethodInvocation(ClientID client_id,
                                       RequestID request_id,
                                       std::unique_ptr<ProtoMessage> args) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;  // client has disconnected by the time the reply came back.

  ClientConnection* client = client_it->second.get();
  std::unique_ptr<RPCFrame> reply_frame(new RPCFrame());
  reply_frame->set_request_id(request_id);
  auto* reply = reply_frame->mutable_msg_invoke_method_reply();
  reply_frame->set_reply_success(!!args);
  reply->set_eof(true);  // TODO(primiano): support streaming replies.
  if (args) {
    std::string reply_proto;
    if (!args->SerializeToString(&reply_proto)) {
      reply_frame->set_reply_success(false);
      reply->set_reply_proto(reply_proto);
    }
  }
  SendRPCFrame(client, std::move(reply_frame));
}

void HostImpl::SendRPCFrame(ClientConnection* client,
                            std::unique_ptr<RPCFrame> reply) {
  uint32_t payload_len = reply ? static_cast<uint32_t>(reply->ByteSize()) : 0;
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (reply) {
    if (!reply->SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
      DCHECK(false);
      payload_len = 0;
    }
  }
  uint32_t enc_size = BYTE_SWAP_TO_LE32(payload_len);
  memcpy(buf.get(), &enc_size, sizeof(uint32_t));

  // TODO(primiano): remember that this is doing non-blocking I/O. What if the
  // socket buffer is full? Maybe we just want to drop this on the floor? Or
  // maybe throttle the send and PostTask the reply later?
  client->sock.Send(buf.get(), sizeof(uint32_t) + payload_len);
}

void HostImpl::AddHandle(HostHandle* handle) {
  handles_.insert(handle);
}

void HostImpl::RemoveHandle(HostHandle* handle) {
  handles_.erase(handle);
}

HostImpl::ClientConnection::~ClientConnection() = default;

}  // namespace protorpc
}  // namespace perfetto
