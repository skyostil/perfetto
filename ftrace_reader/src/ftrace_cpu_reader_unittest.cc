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

#include "base/utils.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "ftrace_event.pb.h"
#include "ftrace_event_bundle.pb.h"

using testing::_;
using testing::Return;
using testing::SaveArg;
using testing::Invoke;

namespace perfetto {

namespace {

const size_t kPageSize = 4096;

std::unique_ptr<uint8_t[]> MakeBuffer(size_t size) {
  return std::unique_ptr<uint8_t[]>(new uint8_t[size]);
}

}  // namespace

class MockRegion : public FtraceCpuReader::Region {
 public:
  MockRegion() {}
  ~MockRegion() {}
  MOCK_METHOD1(DoneWriting, void(uint8_t*));
};

class MockDelegate : public FtraceCpuReader::Delegate {
 public:
  MockDelegate() {}
  ~MockDelegate() {}
  MOCK_METHOD0(NewRegion, FtraceCpuReader::Region*());
};

class BinaryWriter {
 public:
  BinaryWriter(uint8_t* ptr, size_t size) : ptr_(ptr), size_(size) {}

  template <typename T>
  void Write(T t) {
    memcpy(ptr_, &t, sizeof(T));
    ptr_ += sizeof(T);
    PERFETTO_CHECK(ptr_ < ptr_ + size_);
  }

  void WriteEventHeader(uint32_t time_delta, uint32_t entry_type) {
    // Entry header is a packed time delta (d) and type (t):
    // dddddddd dddddddd dddddddd dddttttt
    //    Write<uint32_t>((time_delta << 5) | (entry_type & 0x1f));
    Write<uint32_t>(entry_type);
  }

  void WriteString(const char* s) {
    char c;
    while ((c = *s++)) {
      Write<char>(c);
    }
  }

 private:
  uint8_t* ptr_;
  size_t size_;
};

class FtraceCpuReaderTest : public ::testing::Test {
 public:
  FtraceCpuReaderTest() {}

  virtual ~FtraceCpuReaderTest() {}

 private:
  FtraceCpuReaderTest(const FtraceCpuReaderTest&) = delete;
  FtraceCpuReaderTest& operator=(const FtraceCpuReaderTest&) = delete;
};

TEST_F(FtraceCpuReaderTest, ParseEmpty) {
  std::unique_ptr<uint8_t[]> in_page = MakeBuffer(kPageSize);
  std::unique_ptr<uint8_t[]> out_page = MakeBuffer(kPageSize);

  MockRegion mockRegion;
  mockRegion.start = out_page.get();
  mockRegion.end = out_page.get() + kPageSize;
  MockDelegate mockDelegate;

  uint8_t* out_page_end = nullptr;
  EXPECT_CALL(mockRegion,
              DoneWriting(_));  //.WillOnce(SaveArg<0>(out_page_end));
  EXPECT_CALL(mockDelegate, NewRegion()).WillOnce(Return(&mockRegion));

  BinaryWriter writer(in_page.get(), kPageSize);
  // Timestamp:
  writer.Write<uint64_t>(999);
  // Page length:
  writer.Write<uint64_t>(35);
  // 4 Header:
  writer.WriteEventHeader(1 /* time delta */, 8 /* entry type */);
  // 6 Event type:
  writer.Write<uint16_t>(5);
  // 7 Flags:
  writer.Write<uint8_t>(0);
  // 8 Preempt count:
  writer.Write<uint8_t>(0);
  // 12 PID:
  writer.Write<uint32_t>(72);
  // 20 Instruction pointer:
  writer.Write<uint64_t>(0);
  // 35 String:
  writer.WriteString("Hello, world!\n");

  FtraceCpuReader::ParsePage(42, in_page.get(), &mockDelegate);

  FtraceEventBundle bundle;
  size_t size = out_page_end - out_page.get();
  bundle.ParseFromArray(out_page.get(), static_cast<int>(size));

  EXPECT_EQ(42, bundle.cpu());
  EXPECT_EQ(1, bundle.event_size());
  EXPECT_EQ(72, bundle.event(0).pid());

  google::protobuf::ShutdownProtobufLibrary();
}

}  // namespace perfetto
