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

#include "ftrace_reader/ftrace_cpu_reader.h"

#include <stdint.h>

#include "gtest/gtest.h"
#include "base/utils.h"

namespace perfetto {

namespace {
const size_t kPageSize = 4096;

std::unique_ptr<char[]> MakeBuffer(size_t size) {
  return std::unique_ptr<char[]>(new char[size]);
}

class BinaryWriter {
 public:
  BinaryWriter(char* ptr, size_t size) : ptr_(ptr), size_(size) {
  }

  template <typename T>
  void Write(T t) {
    memcpy(ptr_, &t, sizeof(T));
    ptr_ += sizeof(T);
    PERFETTO_CHECK(ptr_ < ptr_ + size_);
  }

 private:
  char* ptr_;
  size_t size_;
};

} // namespace

class FtraceCpuReaderTest : public ::testing::Test {
 public:
  FtraceCpuReaderTest() {
  }

  virtual ~FtraceCpuReaderTest() {
  }

 private:
  FtraceCpuReaderTest(const FtraceCpuReaderTest&) = delete;
  FtraceCpuReaderTest& operator=(const FtraceCpuReaderTest&) = delete;
};

TEST_F(FtraceCpuReaderTest, ParseEmpty) {
  std::unique_ptr<char[]> in_page = MakeBuffer(kPageSize);
  std::unique_ptr<char[]> out_page = MakeBuffer(kPageSize);
  BinaryWriter writer(out_page.get(), kPageSize);
  writer.Write<uint64_t>(4);

  FtraceRegion region{out_page.get(), out_page.get() + kPageSize};
  FtraceCpuReader::ParsePage(in_page.get(), region);
}

}  // namespace perfetto
