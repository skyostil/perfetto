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

#ifndef FTRACE_READER_FTRACE_CPU_READER_H_
#define FTRACE_READER_FTRACE_CPU_READER_H_

#include <stdint.h>

#include "base/scoped_file.h"
#include "ftrace_event_bundle.pbzero.h"
#include "gtest/gtest_prod.h"

namespace perfetto {

class FtraceCpuReader {
 public:
  FtraceCpuReader(size_t cpu, int fd);

  void Read(pbzero::FtraceEventBundle*);

  int GetFileDescriptor();

  FtraceCpuReader(FtraceCpuReader&&) = default;

 private:
  friend class FtraceCpuReaderTest;
  FRIEND_TEST(FtraceCpuReaderTest, ParseEmpty);

  static void ParsePage(size_t cpu, const uint8_t*, pbzero::FtraceEventBundle*);

  FtraceCpuReader(const FtraceCpuReader&) = delete;
  FtraceCpuReader& operator=(const FtraceCpuReader&) = delete;

  size_t cpu_;
  base::ScopedFile fd_;
};

} // namespace perfetto

#endif  // FTRACE_READER_FTRACE_CPU_READER_H_
