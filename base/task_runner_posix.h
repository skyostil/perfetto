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

#ifndef PERFETTO_BASE_TASK_RUNNER_POSIX_H_
#define PERFETTO_BASE_TASK_RUNNER_POSIX_H_

#include "base/scoped_file.h"
#include "base/task_runner.h"

#include <poll.h>
#include <chrono>
#include <map>
#include <mutex>
#include <queue>

namespace perfetto {
namespace base {

// Runs a task runner on the current thread.
class TaskRunnerPosix : public TaskRunner {
 public:
  TaskRunnerPosix();
  ~TaskRunnerPosix() override;

  void Run();
  void Quit();

  // TaskRunner implementation:
  void PostTask(std::function<void()>) override;
  void PostDelayedTask(std::function<void()>, int delay_ms) override;
  void AddFileDescriptorWatch(int fd, std::function<void()>) override;
  void RemoveFileDescriptorWatch(int fd) override;

 private:
  using TimePoint = std::chrono::time_point<std::chrono::steady_clock>;
  using TimeDuration = std::chrono::milliseconds;
  TimePoint GetTime() const;

  void WakeUp();

  TimeDuration GetDelayToNextTaskLocked() const;
  void RunImmediateTask();
  void RunDelayedTask();
  void RunFileDescriptorWatches();

  ScopedFile control_read_;
  ScopedFile control_write_;

  // --- Begin lock-protected members.
  std::mutex lock_;

  std::deque<std::function<void()>> immediate_tasks_;
  std::multimap<TimePoint, std::function<void()>> delayed_tasks_;
  bool done_ = false;
  // --- End lock-protected members.

  // Since poll(2) is already O(N), there's not much point making these data
  // structures any fancier.
  // TODO: These need to be made double-buffered for thread safety.
  std::vector<std::function<void()>> poll_tasks_;
  std::vector<struct pollfd> poll_fds_;
};

}  // namespace base
}  // namespace perfetto

#endif  // PERFETTO_BASE_TASK_RUNNER_POSIX_H_
