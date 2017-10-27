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

// TODO(primiano): Add ThreadChecker everywhere.

namespace perfetto {
namespace protorpc {

// static
std::shared_ptr<Host> Host::CreateInstance(const char* socket_name,
                                           TaskRunner* task_runner) {
  std::shared_ptr<HostImpl> host(new HostImpl(socket_name, task_runner));
  host->set_weak_ptr(std::weak_ptr<HostImpl>(host));
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
    ClientID client_id = ++last_client_id_;
    client->id = client_id;
    client->sock = std::move(cli_sock);
    clients_[client_id] = std::move(client);
    // TODO use the weak ptr.
    task_runner_->AddFileDescriptorWatch(
        cli_sock_fd, std::bind(&HostImpl::OnDataAvailable, this, client_id));
  }
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

bool HostImpl::ExposeService(const std::shared_ptr<Service>& service) {
  const ServiceDescriptor& desc = service->GetDescriptor();
  if (GetServiceByName(desc.service_name)) {
    DLOG("Duplicate ExposeService(): %s", desc.service_name.c_str());
    return false;
  }
  ServiceID sid = ++last_service_id_;
  services_.emplace(sid, ExposedService{service, sid, desc.service_name});
  return true;
}

const HostImpl::ExposedService* HostImpl::GetServiceByName(
    const std::string& name) {
  for (const auto& it : services_) {
    if (it.second.name == name)
      return &it.second;
  }
  return nullptr;
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
  if (req_frame.msg_case() == RPCFrame::kMsgBindService) {
    return OnBindService(client, req_frame);
  } else if (req_frame.msg_case() == RPCFrame::kMsgInvokeMethod) {
    return OnInvokeMethod(client, req_frame);
  }
  DLOG("Received invalid RPC frame %u from client %" PRIu64,
       req_frame.msg_case(), client_id);
  RPCFrame reply_frame;
  reply_frame.set_request_id(req_frame.request_id());
  reply_frame.set_reply_success(false);
  SendRPCFrame(client, reply_frame);
}

void HostImpl::OnBindService(ClientConnection* client,
                             const RPCFrame& req_frame) {
  // Binding a service doesn't do anything fancy really. It just returns back
  // the service id and the methods ids.
  const RPCFrame::BindService& req = req_frame.msg_bind_service();
  RPCFrame reply_frame;
  reply_frame.set_request_id(req_frame.request_id());
  reply_frame.set_reply_success(false);
  auto* reply = reply_frame.mutable_msg_bind_service_reply();
  const ExposedService* service = GetServiceByName(req.service_name());
  if (service) {
    reply_frame.set_reply_success(true);
    reply->set_service_id(service->id);
    uint32_t method_id = 1;  // method ids start at 1.
    for (const auto& desc_method : service->instance->GetDescriptor().methods) {
      RPCFrame::BindServiceReply::Method* method = reply->add_methods();
      method->set_name(desc_method.name);
      method->set_id(method_id++);
    }
  }
  SendRPCFrame(client, reply_frame);
}

void HostImpl::OnInvokeMethod(ClientConnection* client,
                              const RPCFrame& req_frame) {
  const RPCFrame::InvokeMethod& req = req_frame.msg_invoke_method();
  RPCFrame reply_frame;
  RequestID request_id = req_frame.request_id();
  reply_frame.set_request_id(request_id);
  reply_frame.set_reply_success(false);
  auto svc_it = services_.find(req.service_id());
  if (svc_it == services_.end())
    return SendRPCFrame(client, reply_frame);

  Service* service = svc_it->second.instance.get();
  const ServiceDescriptor& svc = service->GetDescriptor();
  const auto& methods = svc.methods;
  if (req.method_id() <= 0 || req.method_id() > methods.size())
    return SendRPCFrame(client, reply_frame);

  // TODO check whether this is correct.
  const ServiceDescriptor::Method& method = methods[req.method_id() - 1];
  std::unique_ptr<ProtoMessage> req_args(
      method.request_proto_decoder(req.args_proto()));
  if (!req_args)
    return SendRPCFrame(client, reply_frame);

  // TODO here the descriptor or the impl should tell if has_more (for
  // streaming reply). for now hard-coding it to false.

  Deferred<ProtoMessage> reply_handler(method.reply_proto_factory());
  std::weak_ptr<HostImpl> weak_ptr;
  ClientID client_id = client->id;
  reply_handler.Bind(
      [weak_ptr, client_id, request_id](Deferred<ProtoMessage> reply) {
        std::shared_ptr<HostImpl> host = weak_ptr.lock();
        if (host)
          return;  // The host is gone in the meantime.
        host->ReplyToMethodInvocation(client_id, request_id, std::move(reply));
      });

  // TODO post on task runner.
  method.invoker(service, *req_args, std::move(reply_handler));
}

void HostImpl::ReplyToMethodInvocation(ClientID client_id,
                                       RequestID request_id,
                                       Deferred<ProtoMessage> reply) {
  auto client_it = clients_.find(client_id);
  if (client_it == clients_.end())
    return;  // client has disconnected by the time the reply came back.

  ClientConnection* client = client_it->second.get();
  RPCFrame reply_frame;
  reply_frame.set_request_id(request_id);
  reply_frame.set_reply_success(false);

  auto* reply_frame_data = reply_frame.mutable_msg_invoke_method_reply();
  reply_frame_data->set_has_more(reply.has_more());
  if (reply.success()) {
    std::string reply_proto;
    if (reply->SerializeToString(&reply_proto)) {
      reply_frame.set_reply_success(true);
      reply_frame_data->set_reply_proto(reply_proto);
    }
  }
  SendRPCFrame(client, reply_frame);
}

void HostImpl::SendRPCFrame(ClientConnection* client, const RPCFrame& reply) {
  uint32_t payload_len = static_cast<uint32_t>(reply.ByteSize());
  std::unique_ptr<char[]> buf(new char[sizeof(uint32_t) + payload_len]);
  if (!reply.SerializeToArray(buf.get() + sizeof(uint32_t), payload_len)) {
    DCHECK(false);
    payload_len = 0;
  }
  uint32_t enc_size = BYTE_SWAP_TO_LE32(payload_len);
  memcpy(buf.get(), &enc_size, sizeof(uint32_t));

  // TODO(primiano): remember that this is doing non-blocking I/O. What if the
  // socket buffer is full? Maybe we just want to drop this on the floor? Or
  // maybe throttle the send and PostTask the reply later?
  client->sock.Send(buf.get(), sizeof(uint32_t) + payload_len);
}

HostImpl::ClientConnection::~ClientConnection() = default;

}  // namespace protorpc
}  // namespace perfetto
