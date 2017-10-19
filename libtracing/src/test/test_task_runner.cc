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

#include "libtracing/src/test/test_task_runner.h"

#include <stdio.h>
#include <unistd.h>

#include "libtracing/src/core/base.h"

namespace perfetto {

TestTaskRunner::TestTaskRunner() {
  FD_ZERO(&fd_set_);
}

TestTaskRunner::~TestTaskRunner() {}

void TestTaskRunner::Run() {
  for (;;) {
    while (!task_queue_.empty()) {
      std::function<void()> closure = std::move(task_queue_.front());
      task_queue_.pop_front();
      closure();
    }
    int res = select(FD_SETSIZE, &fd_set_, nullptr, nullptr, nullptr);
    if (res < 0) {
      perror("select failed");
      return;
    }
    if (res == 0) {
      printf("select() returned 0, weird. sleeping.\n");
      usleep(100000);
      continue;
    }
    for (int fd = 0; fd < FD_SETSIZE; ++fd) {
      if (!FD_ISSET(fd, &fd_set_))
        continue;
      auto fd_and_callback = watched_fds_.find(fd);
      DCHECK(fd_and_callback != watched_fds_.end());
      fd_and_callback->second();
    }
  }
}

// TaskRunner implementation.
void TestTaskRunner::PostTask(std::function<void()> closure) {
  task_queue_.emplace_back(std::move(closure));
}

void TestTaskRunner::AddFileDescriptorWatch(int fd,
                                            std::function<void()> callback) {
  DCHECK(fd > 0);
  DCHECK(watched_fds_.count(fd) == 0);
  watched_fds_.emplace(fd, std::move(callback));
  FD_SET(fd, &fd_set_);
}

void TestTaskRunner::RemoveFileDescriptorWatch(int fd) {
  DCHECK(fd > 0);
  DCHECK(watched_fds_.count(fd) == 1);
  watched_fds_.erase(fd);
  FD_CLR(fd, &fd_set_);
}
}  // namespace perfetto
