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

#include "base/task_runner_posix.h"

#include <poll.h>
#include <unistd.h>

namespace perfetto {
namespace base {

TaskRunnerPosix::TaskRunnerPosix() {
  int pipe_fds[2];
  if (pipe(pipe_fds) == -1) {
    PERFETTO_DPLOG("pipe()");
    return;
  }
  control_read_.reset(pipe_fds[0]);
  control_write_.reset(pipe_fds[1]);
  // TODO: Why doesn't emplace_back work?
  poll_fds_.push_back({control_read_.get(), POLLIN, 0});
}

TaskRunnerPosix::~TaskRunnerPosix() = default;

TaskRunnerPosix::TimePoint TaskRunnerPosix::GetTime() const {
  return std::chrono::steady_clock::now();
}

void TaskRunnerPosix::WakeUp() {
  // TODO: Only do this if we're not on the main thread.
  if (write(control_write_.get(), nullptr, 0) < 0)
    PERFETTO_DPLOG("write()");
}

void TaskRunnerPosix::Run() {
  while (true) {
    int delay_ms;
    {
      std::lock_guard<std::mutex> lock(lock_);
      if (done_)
        break;
      delay_ms = static_cast<int>(GetDelayToNextTaskLocked().count());
    }
    int ret = 0;
    // Don't start polling until we run out of runnable tasks.
    if (delay_ms)
      ret =
          poll(&poll_fds_[0], static_cast<nfds_t>(poll_fds_.size()), delay_ms);
    if (ret == -1) {
      PERFETTO_DPLOG("poll()");
      return;
    } else if (ret == 0) {
      // Timeout.
      RunImmediateTask();
      RunDelayedTask();
    } else {
      RunImmediateTask();
      RunFileDescriptorWatches();
      RunDelayedTask();
    }
  }
}

void TaskRunnerPosix::Quit() {
  {
    std::lock_guard<std::mutex> lock(lock_);
    done_ = true;
  }
  WakeUp();
}

void TaskRunnerPosix::RunImmediateTask() {
  if (immediate_tasks_.empty())
    return;
  auto task = std::move(immediate_tasks_.front());
  immediate_tasks_.pop_front();
  task();
}

void TaskRunnerPosix::RunDelayedTask() {
  if (delayed_tasks_.empty())
    return;
  auto it = delayed_tasks_.begin();
  if (GetTime() < it->first)
    return;
  auto task = std::move(it->second);
  delayed_tasks_.erase(it);
  task();
}

void TaskRunnerPosix::RunFileDescriptorWatches() {
  for (size_t i = 1; i < poll_fds_.size(); i++) {
    // Note: The control fd has no associated task.
    const auto& task = poll_tasks_[i - 1];
    if (!task) {
      poll_fds_.erase(poll_fds_.begin() + static_cast<ssize_t>(i));
      poll_tasks_.erase(poll_tasks_.begin() + static_cast<ssize_t>(i - 1));
      i--;
      continue;
    }
    if (!(poll_fds_[i].revents & POLLIN))
      continue;
    poll_fds_[i].revents = 0;
    task();
  }
}

TaskRunnerPosix::TimeDuration TaskRunnerPosix::GetDelayToNextTaskLocked()
    const {
  if (!immediate_tasks_.empty())
    return TimeDuration(0);
  if (!delayed_tasks_.empty()) {
    return std::max(TimeDuration(0),
                    std::chrono::duration_cast<TimeDuration>(
                        delayed_tasks_.begin()->first - GetTime()));
  }
  return TimeDuration(-1);
}

void TaskRunnerPosix::PostTask(std::function<void()> task) {
  bool was_empty;
  {
    std::lock_guard<std::mutex> lock(lock_);
    was_empty = immediate_tasks_.empty();
    immediate_tasks_.push_back(std::move(task));
  }
  if (was_empty)
    WakeUp();
}

void TaskRunnerPosix::PostDelayedTask(std::function<void()> task,
                                      int delay_ms) {
  PERFETTO_DCHECK(delay_ms >= 0);
  {
    std::lock_guard<std::mutex> lock(lock_);
    delayed_tasks_.insert(std::make_pair(
        GetTime() + std::chrono::milliseconds(delay_ms), std::move(task)));
  }
  WakeUp();
}

void TaskRunnerPosix::AddFileDescriptorWatch(int fd,
                                             std::function<void()> task) {
  for (const auto& poll_fd : poll_fds_)
    PERFETTO_DCHECK(poll_fd.fd != fd);

  // TODO: Why doesn't emplace_back work?
  poll_fds_.push_back({fd, POLLIN, 0});
  poll_tasks_.emplace_back(std::move(task));
  WakeUp();
}

void TaskRunnerPosix::RemoveFileDescriptorWatch(int fd) {
  for (size_t i = 0; i < poll_fds_.size(); i++) {
    if (poll_fds_[i].fd == fd) {
      // Entry will be remobed at nxet event loop cycle.
      poll_tasks_[i] = nullptr;
      return;
    }
  }
  PERFETTO_DCHECK(false);
}

}  // namespace base
}  // namespace perfetto
