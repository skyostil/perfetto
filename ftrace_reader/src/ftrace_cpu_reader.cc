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

#include <utility>

#include "ftrace_event.pbzero.h"
#include "ftrace_to_proto_translation_table.h"
#include "base/logging.h"

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

const size_t kPageSize = 4096;

template <typename T>
T ReadAndAdvance(const uint8_t** ptr) {
  T ret;
  memcpy(&ret, *ptr, sizeof(T));
  *ptr += sizeof(T);
  return ret;
}

}  // namespace

FtraceCpuReader::FtraceCpuReader(const FtraceToProtoTranslationTable* table,
                                 size_t cpu,
                                 base::ScopedFile fd)
    : table_(table), cpu_(cpu), fd_(std::move(fd)) {}

bool FtraceCpuReader::Read(const Config& config,
                           pbzero::FtraceEventBundle* bundle) {
  if (!fd_)
    return false;

  uint8_t* buffer = GetBuffer();
  // TOOD(hjd): One read() per page may be too many.
  long bytes = PERFETTO_EINTR(read(fd_.get(), buffer, kPageSize));
  if (bytes == -1 || bytes == 0)
    return false;
  PERFETTO_CHECK(bytes <= kPageSize);

  return ParsePage(cpu_, buffer, bytes, bundle);
}

FtraceCpuReader::~FtraceCpuReader() = default;
FtraceCpuReader::FtraceCpuReader(FtraceCpuReader&&) = default;

uint8_t* FtraceCpuReader::GetBuffer() {
  // TODO(primiano): Guard against overflows, like BufferedFrameDeserializer.
  if (!buffer_)
    buffer_ = std::unique_ptr<uint8_t[]>(new uint8_t[kPageSize]);
  return buffer_.get();
}

// The structure of a raw trace buffer page is as follows:
// First a page header:
//   8 bytes of timestamp
//   8 bytes of page length TODO(hjd): other fields also defined here?
// // TODO(hjd): Document rest of format.
// Some information about the layout of the page header is available in user
// space at: /sys/kernel/debug/tracing/events/header_event
// This method is deliberately static so it can be tested independently.
bool FtraceCpuReader::ParsePage(size_t cpu,
                                const uint8_t* ptr,
                                size_t ptr_size,
                                pbzero::FtraceEventBundle* bundle) {
  bundle->set_cpu(cpu);

  // TODO(hjd): Read this format dynamically?
  uint64_t timestamp = ReadAndAdvance<uint64_t>(&ptr);
  uint64_t page_length = ReadAndAdvance<uint64_t>(&ptr) & 0xfffful;
  PERFETTO_CHECK(page_length <= kPageSize - sizeof(uint64_t) * 2);
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
        PERFETTO_DLOG("Padding");
        if (time_delta == 0) {
          // TODO(hjd): Look at the next few bytes for read size;
        }
        PERFETTO_CHECK(false);  // TODO(hjd): Handle
        break;
      }
      case kTypeTimeExtend: {
        // Extend the time delta.
        PERFETTO_DLOG("Extended Time Delta");
        const uint32_t time_delta_ext = ReadAndAdvance<uint32_t>(&ptr);
        (void)time_delta_ext;
        // TODO(hjd): Handle.
        break;
      }
      case kTypeTimeStamp: {
        // Sync time stamp with external clock.
        PERFETTO_DLOG("Time Stamp");
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
        // Type is <=28 so it represents the length of a data record.
        if (type == 0) {
          // TODO(hjd): Look at the next few bytes for real size.
          PERFETTO_CHECK(false);
        }
        const uint8_t* next = ptr + 4 * type;

        uint16_t event_type = ReadAndAdvance<uint16_t>(&ptr);

        // Common headers:
        // TODO(hjd): Read this format dynamically?
        ReadAndAdvance<uint8_t>(&ptr);  // flags
        ReadAndAdvance<uint8_t>(&ptr);  // preempt count
        uint32_t pid = ReadAndAdvance<uint32_t>(&ptr);
        PERFETTO_DLOG("Event type=%d pid=%d", event_type, pid);

        pbzero::FtraceEvent* event = bundle->add_event();
        event->set_pid(pid);

        if (event_type == 5) {
          // Trace Marker Parser
          ReadAndAdvance<uint64_t>(&ptr);  // ip
          const uint8_t* word_start = ptr;
          PERFETTO_DLOG("  marker=%s", word_start);
          while (*ptr != '\0')
            ptr++;
        }

        // Jump to next event.
        ptr = next;
        PERFETTO_DLOG("%zu", ptr - start);
      }
    }
  }
  return true;
}

}  // namespace perfetto
