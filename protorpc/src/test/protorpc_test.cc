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

#include "base/logging.h"
#include "base/test/test_task_runner.h"
#include "protorpc/client.h"
#include "protorpc/host.h"
#include "protorpc/src/test/greeter_impl.h"

#include <stdio.h>

#include "gmock/gmock.h"

namespace perfetto {
namespace protorpc {
namespace {

using protorpc_test::Greeter;
using protorpc_test::GreeterImpl;
using protorpc_test::GreeterProxy;
using protorpc_test::GreeterRequestMsg;
using protorpc_test::GreeterReplyMsg;
using ::testing::Invoke;

const char kSocketName[] = "/tmp/test_protorpc";

class MockEventListener : public ServiceProxy::EventListener {
 public:
  MOCK_METHOD0(OnConnect, void());
  MOCK_METHOD0(OnConnectionFailed, void());
};

int HostMain() {
  unlink(kSocketName);
  base::TestTaskRunner task_runner;
  std::shared_ptr<GreeterImpl> svc(new GreeterImpl());
  std::shared_ptr<Host> host(Host::CreateInstance(kSocketName, &task_runner));
  PERFETTO_CHECK(host->ExposeService(svc));
  host->Start();
  task_runner.Run();
  return 0;
}

int ClientMain() {
  base::TestTaskRunner task_runner;
  std::shared_ptr<Client> client(
      Client::CreateInstance(kSocketName, &task_runner));
  PERFETTO_DCHECK(client);
  std::shared_ptr<GreeterProxy> svc_proxy(new GreeterProxy());
  std::unique_ptr<MockEventListener> event_listener(new MockEventListener);
  auto on_connect = task_runner.GetCheckpointClosure("connected");
  PERFETTO_DLOG("Connecting...");
  EXPECT_CALL(*event_listener, OnConnectionFailed()).WillRepeatedly(Invoke([] {
    PERFETTO_DLOG("Connection failed");
  }));
  EXPECT_CALL(*event_listener, OnConnect()).WillRepeatedly(Invoke([on_connect] {
    PERFETTO_DLOG("Connected");
    on_connect();
  }));
  svc_proxy->set_event_listener(std::move(event_listener));
  client->BindService(svc_proxy);
  task_runner.RunUntilCheckpoint("connected");

  GreeterRequestMsg req;
  req.set_name("client");
  Deferred<GreeterReplyMsg> reply;
  auto checkpoint = task_runner.GetCheckpointClosure("reply1");
  reply.Bind([checkpoint](Deferred<GreeterReplyMsg> r) {
    PERFETTO_DLOG("SayHello() -> %s!",
                  r.success() ? r->message().c_str() : "FAIL");
    checkpoint();
  });
  svc_proxy->SayHello(req, std::move(reply));
  task_runner.RunUntilCheckpoint("reply1");

  auto checkpoint2 = task_runner.GetCheckpointClosure("reply2");
  reply.Bind([checkpoint2](Deferred<GreeterReplyMsg> r) {
    PERFETTO_DLOG("WaveGoodbye() -> %s!",
                  r.success() ? r->message().c_str() : "FAIL");
    checkpoint2();
  });
  svc_proxy->WaveGoodbye(req, std::move(reply));
  task_runner.RunUntilCheckpoint("reply2");
  return 0;
}

}  // namespace
}  // namespace protorpc
}  // namespace perfetto

int main(int argc, char** argv) {
  if (argc == 2 && strcmp(argv[1], "client") == 0)
    return ::perfetto::protorpc::ClientMain();
  if (argc == 2 && strcmp(argv[1], "host") == 0)
    return ::perfetto::protorpc::HostMain();
  fprintf(stderr, "Usage: %s host | client\n", argv[0]);
  return 1;
}
