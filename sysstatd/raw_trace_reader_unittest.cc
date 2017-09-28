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

#include <unistd.h>

#include "gtest/gtest.h"
#include "raw_trace_reader.h"

namespace perfetto {
namespace {

struct PipeEnds {
  int read_fd;
  int write_fd;
};

PipeEnds CreatePipe() {
  int fds[2] = {-1, -1};
  int status = pipe(fds);
  int read_fd = fds[0];
  int write_fd = fds[1];
  EXPECT_NE(status, -1);
  return PipeEnds{read_fd, write_fd};
}

TEST(RawTraceReader, InvalidFd) {
  ssize_t number_of_events = ReadRawPipe(-1);
  ASSERT_EQ(number_of_events, -1);
}

TEST(RawTraceReader, ReadEmpty) {
  PipeEnds ends = CreatePipe();
  ssize_t number_of_events = ReadRawPipe(ends.read_fd);
  ASSERT_EQ(number_of_events, 0);
}

}  // namespace
}  // namespace perfetto
