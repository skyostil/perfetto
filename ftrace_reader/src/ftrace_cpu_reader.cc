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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <memory>

#include "base/utils.h"
#include "delegate_adaptor.h"
#include "ftrace_event.pbzero.h"
#include "ftrace_event_bundle.pbzero.h"
#include "protozero/protozero_message.h"

namespace perfetto {

namespace {

// For further documentation of these constants see the kernel source:
// linux/include/linux/ring_buffer.h
// Some information about the values of these constants are exposed to user
// space at: /sys/kernel/debug/tracing/events/header_event
const uint32_t kTypeDataTypeLengthMax = 28;
const uint32_t kTypePadding = 29;
const uint32_t kTypeTimeExtend = 30;
const uint32_t kTypeTimeStamp = 31;

// TODO(hjd): Read this at run time?
const size_t kPageSize = 4096;

template <typename T>
T ReadAndAdvance(const uint8_t** ptr) {
  T ret;
  memcpy(&ret, *ptr, sizeof(T));
  *ptr += sizeof(T);
  return ret;
}

class DelegateAdaptor : public protozero::ScatteredStreamWriter::Delegate {
 public:
  explicit DelegateAdaptor(FtraceCpuReader::Delegate* delegate);
  ~DelegateAdaptor() override;

  // ScatteredStreamWriter::Delegate implementation.
  protozero::ContiguousMemoryRange GetNewBuffer() override;

  void Done(size_t message_size);

 private:
  FtraceCpuReader::Delegate* delegate_;
  FtraceCpuReader::Region* current_region_;
};

DelegateAdaptor::DelegateAdaptor(FtraceCpuReader::Delegate* delegate)
    : delegate_(delegate), current_region_(nullptr) {}

DelegateAdaptor::~DelegateAdaptor() {}

// ScatteredStreamWriter::Delegate implementation.
protozero::ContiguousMemoryRange DelegateAdaptor::GetNewBuffer() {
  FtraceCpuReader::Region* region = delegate_->NewRegion();
  if (current_region_ != nullptr) {
    // TODO(hjd): handle this case.
    PERFETTO_CHECK(false);
  }
  current_region_ = region;
  return {region->start, region->end};
}

void DelegateAdaptor::Done(size_t message_size) {
  PERFETTO_CHECK(current_region_);
  current_region_->DoneWriting(current_region_->start + message_size);
}

}  // namespace

FtraceCpuReader::Region::~Region() {}
FtraceCpuReader::Delegate::~Delegate() {}

FtraceCpuReader::FtraceCpuReader(uint32_t cpu, int fd)
    : cpu_(cpu), fd_(base::ScopedFile(fd)) {}

void FtraceCpuReader::Read(Delegate* delegate) {
  if (fd_.get() == -1)
    return;

  std::unique_ptr<uint8_t[]> buffer =
      std::unique_ptr<uint8_t[]>(new uint8_t[perfetto::kPageSize]);

  long bytes = PERFETTO_EINTR(read(fd_.get(), buffer.get(), kPageSize));
  if (bytes == -1 || bytes == 0)
    return;

  PERFETTO_DCHECK(bytes == kPageSize);

  ParsePage(cpu_, buffer.get(), delegate);
}

// The structure of a raw trace buffer page is as follows:
// First a page header:
//   8 bytes of timestamp
//   8 bytes of page length TODO(hjd): other fields also defined here?
// // TODO(hjd): Document rest of format.
// Some information about the layout of the page header is available in user
// space at: /sys/kernel/debug/tracing/events/header_event
// This method is deliberately static so it can be tested independently.
// static
void FtraceCpuReader::ParsePage(uint32_t cpu,
                                const uint8_t* ptr,
                                Delegate* delegate) {
  DelegateAdaptor adaptor(delegate);
  protozero::ScatteredStreamWriter stream_writer(&adaptor);
  pbzero::FtraceEventBundle message;
  message.Reset(&stream_writer);
  message.set_cpu(cpu);

  // TODO(hjd): Read this format dynamically?
  uint64_t timestamp = ReadAndAdvance<uint64_t>(&ptr);
  uint64_t page_length = ReadAndAdvance<uint64_t>(&ptr) & 0xfffful;
  PERFETTO_CHECK(page_length <= kPageSize - 64 * 2);
  const uint8_t* const start = ptr;
  const uint8_t* const end = ptr + page_length;

  // TODO(hjd): Remove.
  (void)start;
  (void)timestamp;

  while (ptr < end) {
    const uint32_t event_header = ReadAndAdvance<uint32_t>(&ptr);
    const uint32_t type = event_header & 0x1f;
    const uint32_t time_delta = event_header >> 5;

    switch (type) {
      case kTypePadding: {
        // Left over page padding or discarded event.
        printf("Padding\n");
        if (time_delta == 0) {
          // TODO(hjd): Look at the next few bytes for read size;
        }
        PERFETTO_CHECK(false);  // TODO(hjd): Handle
        break;
      }
      case kTypeTimeExtend: {
        // Extend the time delta.
        printf("Extended Time Delta\n");
        const uint32_t time_delta_ext = ReadAndAdvance<uint32_t>(&ptr);
        (void)time_delta_ext;
        // TODO(hjd): Handle.
        break;
      }
      case kTypeTimeStamp: {
        // Sync time stamp with external clock.
        printf("Time Stamp\n");
        const uint64_t tv_nsec = ReadAndAdvance<uint64_t>(&ptr);
        const uint64_t tv_sec = ReadAndAdvance<uint64_t>(&ptr);
        // TODO(hjd): Handle.

        // TODO(hjd): Remove.
        (void)tv_nsec;
        (void)tv_sec;
        break;
      }
      // Data record:
      default: {
        PERFETTO_CHECK(type <= kTypeDataTypeLengthMax);
        // Where type is <=28 it represents the length of a data record
        if (type == 0) {
          PERFETTO_CHECK(
              false);  // TODO(hjd): Look at the next few bytes for real size.
        }
        const uint8_t* next = ptr + 4 * type;

        uint16_t event_type = ReadAndAdvance<uint16_t>(&ptr);

        // Common headers:
        // TODO(hjd): Read this format dynamically?
        ReadAndAdvance<uint8_t>(&ptr);  // flags
        ReadAndAdvance<uint8_t>(&ptr);  // preempt count
        uint32_t pid = ReadAndAdvance<uint32_t>(&ptr);
        printf("Event type=%d pid=%d\n", event_type, pid);

        pbzero::FtraceEvent* event = message.add_event();
        event->set_pid(pid);

        if (event_type == 5) {
          // Trace Marker Parser
          ReadAndAdvance<uint64_t>(&ptr);  // ip
          const uint8_t* word_start = ptr;
          printf("  marker=%s", word_start);
          while (*ptr != '\0')
            ptr++;
        }

        // Jump to next event.
        ptr = next;
        printf("%zu\n", ptr - start);
      }
    }
  }
  size_t message_size = message.Finalize();
  adaptor.Done(message_size);
}

}  // namespace perfetto
