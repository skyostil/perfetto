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

#include "protorpc/src/unix_socket.h"

#include <stdio.h>
#include <sys/mman.h>

#include "base/build_config.h"
#include "base/logging.h"
#include "base/test/test_task_runner.h"
#include "base/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace perfetto {
namespace protorpc {
namespace {

using ::testing::_;
using ::testing::Invoke;
using ::testing::Mock;

// Mac OS X doesn't support abstract (i.e. unnamed) sockets.
#if BUILDFLAG(OS_MACOSX)
static const char kSocketName[] = "/tmp/test_socket";
void UnlinkSocket() {
  unlink(kSocketName);
}
#else
static const char kSocketName[] = "@test_socket";
void UnlinkSocket() {}
#endif

class MockEventListener : public UnixSocket::EventListener {
 public:
  MOCK_METHOD2(OnNewIncomingConnection, void(UnixSocket*, UnixSocket*));
  MOCK_METHOD2(OnConnect, void(UnixSocket*, bool));
  MOCK_METHOD1(OnDisconnect, void(UnixSocket*));
  MOCK_METHOD1(OnDataAvailable, void(UnixSocket*));

  // GMock doesn't support mocking methods with non-copiable args.
  void OnNewIncomingConnection(
      UnixSocket* self,
      std::unique_ptr<UnixSocket> new_connection) override {
    new_connections.emplace_back(std::move(new_connection));
    OnNewIncomingConnection(self, new_connections.back().get());
  }

  std::vector<std::unique_ptr<UnixSocket>> new_connections;
};

class UnixSocketTest : public ::testing::Test {
 protected:
  void SetUp() override { UnlinkSocket(); }
  void TearDown() override { UnlinkSocket(); }

  base::TestTaskRunner task_runner_;
  MockEventListener event_listener_;
};

TEST_F(UnixSocketTest, ConnectionFailureIfUnreachable) {
  UnixSocket cli(&event_listener_, &task_runner_);
  ASSERT_FALSE(cli.is_connected());
  ASSERT_FALSE(cli.Connect(kSocketName));
  // Make sure we don't see any unexpected callbacks.
  task_runner_.RunUntilIdle();
}

// Both server and client should see a Shutdown if the new server connection is
// dropped immediately as it is created.
TEST_F(UnixSocketTest, ConnectionImmediatelyDropped) {
  UnixSocket cli(&event_listener_, &task_runner_);
  UnixSocket srv(&event_listener_, &task_runner_);
  ASSERT_TRUE(srv.Listen(kSocketName));

  // The server will immediately shutdown the connection.
  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .WillOnce(Invoke([this](UnixSocket*, UnixSocket* new_conn) {
        EXPECT_CALL(event_listener_, OnDisconnect(new_conn));
        new_conn->Shutdown();
      }));

  auto checkpoint = task_runner_.GetCheckpointClosure("cli_connected");
  EXPECT_CALL(event_listener_, OnConnect(&cli, true))
      .WillOnce(Invoke([checkpoint](UnixSocket*, bool) { checkpoint(); }));
  ASSERT_TRUE(cli.Connect(kSocketName));
  task_runner_.RunUntilCheckpoint("cli_connected");

  EXPECT_FALSE(cli.Send("whatever"));
  EXPECT_CALL(event_listener_, OnDisconnect(&cli));
  task_runner_.RunUntilIdle();
}

TEST_F(UnixSocketTest, ClientServerExchangeData) {
  UnixSocket srv(&event_listener_, &task_runner_);
  UnixSocket cli(&event_listener_, &task_runner_);
  ASSERT_TRUE(srv.Listen(kSocketName));
  ASSERT_TRUE(srv.is_listening());
  ASSERT_FALSE(srv.is_connected());
  EXPECT_CALL(event_listener_, OnConnect(&cli, true));
  auto checkpoint = task_runner_.GetCheckpointClosure("cli_connected");
  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .WillOnce(Invoke([this, checkpoint](UnixSocket*, UnixSocket* srv_conn) {
        EXPECT_CALL(event_listener_, OnDisconnect(srv_conn));
        checkpoint();
      }));
  ASSERT_TRUE(cli.Connect(kSocketName));
  task_runner_.RunUntilCheckpoint("cli_connected");

  ASSERT_FALSE(event_listener_.new_connections.empty());
  UnixSocket& srv_conn = *event_listener_.new_connections.back();
  ASSERT_TRUE(cli.is_connected());
  EXPECT_CALL(event_listener_, OnDataAvailable(&cli))
      .WillOnce(
          Invoke([](UnixSocket* s) { ASSERT_EQ("srv>cli", s->RecvString()); }));
  EXPECT_CALL(event_listener_, OnDataAvailable(&srv_conn))
      .WillOnce(
          Invoke([](UnixSocket* s) { ASSERT_EQ("cli>srv", s->RecvString()); }));
  ASSERT_TRUE(cli.Send("cli>srv"));
  ASSERT_TRUE(srv_conn.Send("srv>cli"));
  task_runner_.RunUntilIdle();

  // Check that Send/Recv() fails gracefully on a closed socket.
  EXPECT_CALL(event_listener_, OnDisconnect(&cli));
  cli.Shutdown();
  char msg[4];
  ASSERT_EQ(0u, cli.Recv(&msg, sizeof(msg)));
  ASSERT_EQ("", cli.RecvString());
  ASSERT_EQ(0u, srv_conn.Recv(&msg, sizeof(msg)));
  ASSERT_EQ("", srv_conn.RecvString());
  ASSERT_FALSE(cli.Send("foo"));
  ASSERT_FALSE(srv_conn.Send("bar"));
  srv.Shutdown();
  task_runner_.RunUntilIdle();
}

TEST_F(UnixSocketTest, SeveralClients) {
  UnixSocket srv(&event_listener_, &task_runner_);
  constexpr size_t kNumClients = 32;
  std::unique_ptr<UnixSocket> cli[kNumClients];
  ASSERT_TRUE(srv.Listen(kSocketName));

  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .Times(kNumClients)
      .WillRepeatedly(Invoke([this](UnixSocket*, UnixSocket* s) {
        ASSERT_TRUE(s->Send("srv>cli"));
        EXPECT_CALL(event_listener_, OnDataAvailable(s))
            .WillOnce(Invoke(
                [](UnixSocket* t) { ASSERT_EQ("cli>srv", t->RecvString()); }));
      }));

  for (size_t i = 0; i < kNumClients; i++) {
    cli[i].reset(new UnixSocket(&event_listener_, &task_runner_));

    EXPECT_CALL(event_listener_, OnConnect(cli[i].get(), true))
        .WillOnce(Invoke(
            [](UnixSocket* s, bool) { ASSERT_TRUE(s->Send("cli>srv")); }));

    auto checkpoint = task_runner_.GetCheckpointClosure(std::to_string(i));
    EXPECT_CALL(event_listener_, OnDataAvailable(cli[i].get()))
        .WillOnce(Invoke([checkpoint](UnixSocket* s) {
          ASSERT_EQ("srv>cli", s->RecvString());
          checkpoint();
        }));

    ASSERT_TRUE(cli[i]->Connect(kSocketName));
  }

  for (size_t i = 0; i < kNumClients; i++) {
    task_runner_.RunUntilCheckpoint(std::to_string(i));
    Mock::VerifyAndClearExpectations(cli[i].get());
  }
}

// Creates two processes. The server process creates a file and passes it over
// the socket to the client. Both processes mmap the file and check that
// see the same contents.
TEST_F(UnixSocketTest, SharedMemory) {
  pid_t pid = fork();
  ASSERT_GE(pid, 0);
  constexpr size_t kTmpSize = 4096;

  if (pid == 0) {
    FILE* tmp = tmpfile();
    ASSERT_NE(nullptr, tmp);
    int tmp_fd = fileno(tmp);
    ASSERT_FALSE(ftruncate(tmp_fd, kTmpSize));
    char* mem = reinterpret_cast<char*>(
        mmap(nullptr, kTmpSize, PROT_READ | PROT_WRITE, MAP_SHARED, tmp_fd, 0));
    ASSERT_NE(nullptr, mem);
    memcpy(mem, "shm rocks", 10);

    UnixSocket srv(&event_listener_, &task_runner_);
    ASSERT_TRUE(srv.Listen(kSocketName));
    auto checkpoint = task_runner_.GetCheckpointClosure("changed");
    EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
        .WillOnce(Invoke(
            [this, tmp_fd, checkpoint, mem](UnixSocket*, UnixSocket* new_conn) {
              ASSERT_TRUE(new_conn->Send("txfd", 5, tmp_fd));
              // Wait for the client to change this again.
              EXPECT_CALL(event_listener_, OnDataAvailable(new_conn))
                  .WillOnce(Invoke([checkpoint, mem](UnixSocket* s) {
                    ASSERT_EQ("change notify", s->RecvString());
                    ASSERT_STREQ("rock more", mem);
                    checkpoint();
                  }));
            }));
    task_runner_.RunUntilCheckpoint("changed");
  } else {
    UnixSocket cli(&event_listener_, &task_runner_);
    EXPECT_CALL(event_listener_, OnConnect(&cli, true));
    auto checkpoint = task_runner_.GetCheckpointClosure("changed");
    EXPECT_CALL(event_listener_, OnDataAvailable(&cli))
        .WillOnce(Invoke([checkpoint](UnixSocket* s) {
          char msg[32];
          base::ScopedFile fd;
          ASSERT_EQ(5u, s->Recv(msg, sizeof(msg), &fd));
          ASSERT_STREQ("txfd", msg);
          ASSERT_TRUE(fd);
          char* mem = reinterpret_cast<char*>(mmap(
              nullptr, kTmpSize, PROT_READ | PROT_WRITE, MAP_SHARED, *fd, 0));
          ASSERT_NE(nullptr, mem);
          mem[9] = '\0';  // Just to get a clean error in case of test failure.
          ASSERT_STREQ("shm rocks", mem);

          // Now change the shared memory and ping the other process.
          memcpy(mem, "rock more", 10);
          ASSERT_TRUE(s->Send("change notify"));
          checkpoint();
        }));
    for (int attempt = 1; attempt <= 10; attempt++) {
      if (cli.Connect(kSocketName))
        break;
      usleep(5000 * attempt);
    }
    task_runner_.RunUntilCheckpoint("changed");
    int st = 0;
    PERFETTO_EINTR(waitpid(pid, &st, 0));
    ASSERT_FALSE(WIFSIGNALED(st)) << "Server died with signal " << WTERMSIG(st);
    EXPECT_TRUE(WIFEXITED(st));
    ASSERT_EQ(0, WEXITSTATUS(st));
  }
}

// TODO(primiano): add a test to check that in the case of a peer sending a fd
// and the other end just doing a recv (without taking it), the fd is closed
// and not left around.

// TODO out of buffer test.

}  // namespace
}  // namespace protorpc
}  // namespace perfetto
