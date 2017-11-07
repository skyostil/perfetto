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

#include "ftrace_reader/ftrace_reader.h"

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ftrace_paths.h"

namespace perfetto {

FtraceReader::FtraceReader()
    : paths_(new FtracePaths("/sys/kernel/debug/tracing/")),
      controller_(paths_.get()) {}

FtraceReader::~FtraceReader() = default;

FtraceController* FtraceReader::GetController() const {
  return &controller_;
}

const FtraceCpuReader* FtraceReader::GetCpuReader(size_t cpu) const {
  PERFETTO_CHECK(cpu < NumberOfCpus());
  if (!readers_.count(cpu)) {
    int fd = open(paths_.get()->TracePipeRaw(cpu).c_str(), O_RDONLY);
    if (fd == -1)
      return nullptr;
    readers_.emplace(cpu, FtraceCpuReader(cpu, fd));
  }

  return &readers_.at(cpu);
}

size_t FtraceReader::NumberOfCpus() const {
  return sysconf(_SC_NPROCESSORS_CONF);
}

}  // namespace perfetto
