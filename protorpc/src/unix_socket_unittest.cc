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
  cli.Connect(kSocketName);
  auto checkpoint = task_runner_.GetCheckpointClosure("failure");
  EXPECT_CALL(event_listener_, OnConnect(&cli, false))
      .WillOnce(Invoke([checkpoint](UnixSocket*, bool) { checkpoint(); }));
  task_runner_.RunUntilCheckpoint("failure");
}

// Both server and client should see a Shutdown if the new server connection is
// dropped immediately by the server as it is created.
TEST_F(UnixSocketTest, ConnectionImmediatelyDroppedByServer) {
  UnixSocket cli(&event_listener_, &task_runner_);
  UnixSocket srv(&event_listener_, &task_runner_);
  ASSERT_TRUE(srv.Listen(kSocketName));

  // The server will immediately shutdown the connection.
  auto srv_did_shutdown = task_runner_.GetCheckpointClosure("srv_did_shutdown");
  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .WillOnce(
          Invoke([this, srv_did_shutdown](UnixSocket*, UnixSocket* new_conn) {
            EXPECT_CALL(event_listener_, OnDisconnect(new_conn));
            new_conn->Shutdown();
            srv_did_shutdown();
          }));

  auto checkpoint = task_runner_.GetCheckpointClosure("cli_connected");
  EXPECT_CALL(event_listener_, OnConnect(&cli, true))
      .WillOnce(Invoke([checkpoint](UnixSocket*, bool) { checkpoint(); }));
  cli.Connect(kSocketName);
  task_runner_.RunUntilCheckpoint("cli_connected");
  task_runner_.RunUntilCheckpoint("srv_did_shutdown");

  // Trying to send something will trigger the disconnection notification.
  auto cli_disconnected = task_runner_.GetCheckpointClosure("cli_disconnected");
  EXPECT_CALL(event_listener_, OnDisconnect(&cli))
      .WillOnce(
          Invoke([cli_disconnected](UnixSocket*) { cli_disconnected(); }));
  EXPECT_FALSE(cli.Send("whatever"));
  task_runner_.RunUntilCheckpoint("cli_disconnected");
}

TEST_F(UnixSocketTest, ClientAndServerExchangeData) {
  UnixSocket srv(&event_listener_, &task_runner_);
  UnixSocket cli(&event_listener_, &task_runner_);
  ASSERT_TRUE(srv.Listen(kSocketName));
  ASSERT_TRUE(srv.is_listening());
  ASSERT_FALSE(srv.is_connected());
  EXPECT_CALL(event_listener_, OnConnect(&cli, true));
  auto cli_connected = task_runner_.GetCheckpointClosure("cli_connected");
  auto srv_disconnected = task_runner_.GetCheckpointClosure("srv_disconnected");
  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .WillOnce(Invoke([this, cli_connected, srv_disconnected](
                           UnixSocket*, UnixSocket* srv_conn) {
        EXPECT_CALL(event_listener_, OnDisconnect(srv_conn))
            .WillOnce(Invoke(
                [srv_disconnected](UnixSocket*) { srv_disconnected(); }));
        cli_connected();
      }));
  cli.Connect(kSocketName);
  task_runner_.RunUntilCheckpoint("cli_connected");

  ASSERT_FALSE(event_listener_.new_connections.empty());
  UnixSocket& srv_conn = *event_listener_.new_connections.back();
  ASSERT_TRUE(cli.is_connected());

  auto cli_did_recv = task_runner_.GetCheckpointClosure("cli_did_recv");
  EXPECT_CALL(event_listener_, OnDataAvailable(&cli))
      .WillOnce(Invoke([cli_did_recv](UnixSocket* s) {
        ASSERT_EQ("srv>cli", s->RecvString());
        cli_did_recv();
      }));

  auto srv_did_recv = task_runner_.GetCheckpointClosure("srv_did_recv");
  EXPECT_CALL(event_listener_, OnDataAvailable(&srv_conn))
      .WillOnce(Invoke([srv_did_recv](UnixSocket* s) {
        ASSERT_EQ("cli>srv", s->RecvString());
        srv_did_recv();
      }));
  ASSERT_TRUE(cli.Send("cli>srv"));
  ASSERT_TRUE(srv_conn.Send("srv>cli"));
  task_runner_.RunUntilCheckpoint("cli_did_recv");
  task_runner_.RunUntilCheckpoint("srv_did_recv");

  // Check that Send/Recv() fails gracefully on a closed socket.
  auto cli_disconnected = task_runner_.GetCheckpointClosure("cli_disconnected");
  EXPECT_CALL(event_listener_, OnDisconnect(&cli))
      .WillOnce(
          Invoke([cli_disconnected](UnixSocket*) { cli_disconnected(); }));
  cli.Shutdown();
  char msg[4];
  ASSERT_EQ(0u, cli.Recv(&msg, sizeof(msg)));
  ASSERT_EQ("", cli.RecvString());
  ASSERT_EQ(0u, srv_conn.Recv(&msg, sizeof(msg)));
  ASSERT_EQ("", srv_conn.RecvString());
  ASSERT_FALSE(cli.Send("foo"));
  ASSERT_FALSE(srv_conn.Send("bar"));
  srv.Shutdown();
  task_runner_.RunUntilCheckpoint("cli_disconnected");
  task_runner_.RunUntilCheckpoint("srv_disconnected");
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

    cli[i]->Connect(kSocketName);
  }

  for (size_t i = 0; i < kNumClients; i++) {
    task_runner_.RunUntilCheckpoint(std::to_string(i));
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(cli[i].get()));
  }
}

// Creates two processes. The server process creates a file and passes it over
// the socket to the client. Both processes mmap the file and check that
// see the same contents.
TEST_F(UnixSocketTest, SharedMemory) {
  int pipes[2];
  ASSERT_EQ(0, pipe(pipes));

  pid_t pid = fork();
  ASSERT_GE(pid, 0);
  constexpr size_t kTmpSize = 4096;

  if (pid == 0) {
    // Child process.
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
    // Signal the other process that it can connect.
    ASSERT_EQ(1, PERFETTO_EINTR(write(pipes[1], ".", 1)));
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
    ASSERT_TRUE(Mock::VerifyAndClearExpectations(&event_listener_));
    _exit(0);
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
    char sync_cmd = '\0';
    ASSERT_EQ(1, PERFETTO_EINTR(read(pipes[0], &sync_cmd, 1)));
    ASSERT_EQ('.', sync_cmd);
    cli.Connect(kSocketName);
    task_runner_.RunUntilCheckpoint("changed");
    int st = 0;
    PERFETTO_EINTR(waitpid(pid, &st, 0));
    ASSERT_FALSE(WIFSIGNALED(st)) << "Server died with signal " << WTERMSIG(st);
    EXPECT_TRUE(WIFEXITED(st));
    ASSERT_EQ(0, WEXITSTATUS(st));
  }
}

constexpr size_t kAtomicWrites_FrameSize = 1123;
bool AtomicWrites_SendAttempt(UnixSocket* s,
                              base::TaskRunner* task_runner,
                              int num_frame) {
  char buf[kAtomicWrites_FrameSize];
  memset(buf, static_cast<char>(num_frame), sizeof(buf));
  if (s->Send(buf, sizeof(buf)))
    return true;
  task_runner->PostTask(
      std::bind(&AtomicWrites_SendAttempt, s, task_runner, num_frame));
  return false;
}

// Creates a client-server pair. The client writes data aggressively. On each
// attempt, the client sends a buffer filled with a unique number (0 to
// kNumFrames). The client is extremely aggressive and When the Send() fails
// ist just keeps re-posting the Send. We are deliberately trying to fill the
// output buffer, so we expect some of them to necessarily fail. The server
// verifies that we receive one and exactly one of each frame, and verifies that
// they are not trucated.
TEST_F(UnixSocketTest, AtomicWrites) {
  UnixSocket srv(&event_listener_, &task_runner_);
  UnixSocket cli(&event_listener_, &task_runner_);
  ASSERT_TRUE(srv.Listen(kSocketName));
  static constexpr int kNumFrames = 127;

  auto all_frames_done = task_runner_.GetCheckpointClosure("all_frames_done");
  std::set<int> received_iterations;
  EXPECT_CALL(event_listener_, OnNewIncomingConnection(&srv, _))
      .WillOnce(Invoke([this, &received_iterations, all_frames_done](
                           UnixSocket*, UnixSocket* srv_conn) {
        EXPECT_CALL(event_listener_, OnDataAvailable(srv_conn))
            .WillRepeatedly(
                Invoke([&received_iterations, all_frames_done](UnixSocket* s) {
                  char buf[kAtomicWrites_FrameSize];
                  size_t res = s->Recv(buf, sizeof(buf));
                  if (res == 0)
                    return;  // Spurious select(), could happen.
                  ASSERT_EQ(kAtomicWrites_FrameSize, res);
                  // Check that we didn't get two truncated frames.
                  for (size_t i = 0; i < sizeof(buf); i++)
                    ASSERT_EQ(buf[0], buf[i]);
                  ASSERT_EQ(0u, received_iterations.count(buf[0]));
                  received_iterations.insert(buf[0]);
                  if (received_iterations.size() == kNumFrames)
                    all_frames_done();
                }));
      }));

  auto cli_connected = task_runner_.GetCheckpointClosure("cli_connected");
  EXPECT_CALL(event_listener_, OnConnect(&cli, true))
      .WillOnce(
          Invoke([cli_connected](UnixSocket*, bool) { cli_connected(); }));

  cli.Connect(kSocketName);
  task_runner_.RunUntilCheckpoint("cli_connected");
  ASSERT_TRUE(cli.is_connected());

  bool did_requeue = false;
  for (int i = 0; i < kNumFrames; i++)
    did_requeue |= !AtomicWrites_SendAttempt(&cli, &task_runner_, i);

  // We expect that at least one of the kNumFrames didn't fit in the socket
  // buffer and was re-posted, otherwise this entire test would be pointless.
  ASSERT_TRUE(did_requeue);

  task_runner_.RunUntilCheckpoint("all_frames_done");
}

// TODO(primiano): add a test to check that in the case of a peer sending a fd
// and the other end just doing a recv (without taking it), the fd is closed and
// not left around.

// TODO(primiano); add a test to check that a socket can be reused after
// Shutdown(),

// TODO(primiano): add a test to check that OnDisconnect() is called in all
// possible cases.

}  // namespace
}  // namespace protorpc
}  // namespace perfetto
