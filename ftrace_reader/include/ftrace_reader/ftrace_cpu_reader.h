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

#include "base/scoped_file.h"
#include "gtest/gtest_prod.h"

namespace perfetto {

struct FtraceRegion {
  char* start;
  char* end;
};

class FtraceCpuReader {
 public:
  FtraceCpuReader(int fd);

  void Read(FtraceRegion region);
  int GetFileDescriptor();

 private:
  friend class FtraceCpuReaderTest;
  FRIEND_TEST(FtraceCpuReaderTest, ParseEmpty);

  static void ParsePage(const char* ptr, FtraceRegion region);

  FtraceCpuReader(const FtraceCpuReader&) = delete;
  FtraceCpuReader& operator=(const FtraceCpuReader&) = delete;

  base::ScopedFile fd_;
};

} // namespace perfetto

#endif  // FTRACE_READER_FTRACE_CPU_READER_H_
