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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_CORE_TASK_RUNNER_PROXY_H_
#define LIBTRACING_INCLUDE_LIBTRACING_CORE_TASK_RUNNER_PROXY_H_

#include <functional>

namespace perfetto {

// The embedder is supposed to subclass this to allow the execution of
// libtracing on its own message loop.

// TODO we should provide a reference implementation that just spins a dedicated
// thread. For the moment the only implementation is the PoorManTaskRunner in
// unix_test.cc .
class TaskRunnerProxy {
 public:
  virtual ~TaskRunnerProxy() {}

  virtual void PostTask(std::function<void()>) = 0;
  virtual void AddFileDescriptorWatch(int fd, std::function<void()>) = 0;
  virtual void RemoveFileDescriptorWatch(int fd) = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_TASK_RUNNER_PROXY_H_
