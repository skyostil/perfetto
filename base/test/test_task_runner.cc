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

#include "base/test/test_task_runner.h"

#include <stdio.h>
#include <unistd.h>

#include <chrono>

#include "base/logging.h"

// TODO: the current implementation quite hacky as it keeps waking up every 1ms.

namespace perfetto {
namespace base {

TestTaskRunner::TestTaskRunner() = default;

TestTaskRunner::~TestTaskRunner() = default;

size_t TestTaskRunner::RunCurrentTasks() {
  size_t num_tasks = task_queue_.size();
  for (size_t i = 0; i < num_tasks; i++) {
    std::function<void()> closure = std::move(task_queue_.front());
    task_queue_.pop_front();
    closure();
  }
  size_t num_watches_ran = RunFileDescriptorWatches(100);
  return num_tasks + num_watches_ran;
}

void TestTaskRunner::Run() {
  for (;;)
    RunCurrentTasks();
}

void TestTaskRunner::RunUntilIdle() {
  while (RunCurrentTasks() > 0) {
  }
}

void TestTaskRunner::RunUntilCheckpoint(const std::string& checkpoint,
                                        int timeout_ms) {
  PERFETTO_DCHECK(checkpoints_.count(checkpoint) == 1);
  auto tstart = std::chrono::system_clock::now();
  auto deadline = tstart + std::chrono::milliseconds(timeout_ms);
  while (!checkpoints_[checkpoint]) {
    RunCurrentTasks();
    if (std::chrono::system_clock::now() > deadline) {
      printf("[TestTaskRunner] Failed to reach checkpoint \"%s\"\n",
             checkpoint.c_str());
      abort();
    }
  }
}

std::function<void()> TestTaskRunner::GetCheckpointClosure(
    const std::string& checkpoint) {
  PERFETTO_DCHECK(checkpoints_.count(checkpoint) == 0);
  auto checkpoint_iter = checkpoints_.emplace(checkpoint, false);
  return [checkpoint_iter] { checkpoint_iter.first->second = true; };
}

size_t TestTaskRunner::RunFileDescriptorWatches(int timeout_ms) {
  struct timeval timeout;
  timeout.tv_usec = (timeout_ms % 1000) * 1000L;
  timeout.tv_sec = static_cast<time_t>(timeout_ms / 1000);
  int max_fd = 0;
  fd_set fds_in = {};
  fd_set fds_err = {};
  for (const auto& it : watched_fds_) {
    FD_SET(it.first, &fds_in);
    FD_SET(it.first, &fds_err);
    max_fd = std::max(max_fd, it.first);
  }
  int res = select(max_fd + 1, &fds_in, nullptr, &fds_err, &timeout);
  if (res < 0) {
    perror("select() failed");
    abort();
  }
  if (res == 0)
    return 0;  // timeout
  size_t num_watches_ran = 0;
  for (int fd = 0; fd <= max_fd; ++fd) {
    if (!FD_ISSET(fd, &fds_in) && !FD_ISSET(fd, &fds_err)) {
      continue;
    }
    auto fd_and_callback = watched_fds_.find(fd);
    PERFETTO_DCHECK(fd_and_callback != watched_fds_.end());
    num_watches_ran++;
    fd_and_callback->second();
  }
  return num_watches_ran;
}

// TaskRunner implementation.
void TestTaskRunner::PostTask(std::function<void()> closure) {
  task_queue_.emplace_back(std::move(closure));
}

void TestTaskRunner::AddFileDescriptorWatch(int fd,
                                            std::function<void()> callback) {
  PERFETTO_DCHECK(fd >= 0);
  PERFETTO_DCHECK(watched_fds_.count(fd) == 0);
  watched_fds_.emplace(fd, std::move(callback));
}

void TestTaskRunner::RemoveFileDescriptorWatch(int fd) {
  PERFETTO_DCHECK(fd >= 0);
  PERFETTO_DCHECK(watched_fds_.count(fd) == 1);
  watched_fds_.erase(fd);
}

}  // namespace base
}  // namespace perfetto
