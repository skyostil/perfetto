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

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "libftrace/ftrace.h"

namespace perfetto {

namespace {

#define CHECK_ARGS(COND, ERR)                                          \
  "FAILED CHECK(%s) @ %s:%d (errno: %s)\n", #COND, __FILE__, __LINE__, \
      strerror(ERR)

#define CHECK(x)                              \
  do {                                        \
    if (!(x)) {                               \
      const int e = errno;                    \
      fprintf(stderr, "\n" CHECK_ARGS(x, e)); \
      fflush(stderr);                         \
      _exit(1);                               \
    }                                         \
  } while (0)

// This directory contains the 'format' and 'enable' files for each event.
// These are nested like so: group_name/event_name/{format, enable}
const char* kTraceEventPath = "/sys/kernel/debug/tracing/events/";

// Reading this file produces human readable trace output.
// Writing to this file clears all trace buffers for all CPUS.
const char* kTracePath = "/sys/kernel/debug/tracing/trace";

// Writing to this file injects an event into the trace buffer.
const char* kTraceMarkerPath = "/sys/kernel/debug/tracing/trace_marker";

// For further documentation of these constants see the kernel source:
// linux/include/linux/ring_buffer.h
// Some information about the values of these constants are exposed to user
// space at: /sys/kernel/debug/tracing/events/header_event
const uint32_t kTypeDataTypeLengthMax = 28;
const uint32_t kTypePadding = 29;
const uint32_t kTypeTimeExtend = 30;
const uint32_t kTypeTimeStamp = 31;

constexpr size_t length(const char* str) {
  return (*str == 0) ? 0 : length(str + 1) + 1;
}

template <typename T>
T Read(const char** ptr) {
  T ret;
  memcpy(reinterpret_cast<char*>(&ret), *ptr, sizeof(T));
  *ptr += sizeof(T);
  return ret;
}

bool WriteToFile(const char* path, const char* str) {
  bool success = true;
  int fd = open(path, O_WRONLY);
  if (fd == -1)
    return false;

  if (write(fd, str, strlen(str)) == -1)
    success = false;  // Continue to try to close file.

  if (close(fd) == -1)
    return false;

  return success;
}

}  // namespace

void ClearTrace() {
  int fd = open(kTracePath, O_WRONLY | O_TRUNC);
  if (fd == -1) {
    fprintf(stderr, "Could not open %s\n", kTracePath);
    return;
  }

  if (close(fd) == -1) {
    fprintf(stderr, "Could not close %s\n", kTracePath);
  }
}

void WriteTraceMarker(const char* str) {
  int fd = open(kTraceMarkerPath, O_WRONLY);
  if (fd == -1) {
    fprintf(stderr, "Could not open %s\n", kTraceMarkerPath);
    return;
  }

  if (write(fd, str, strlen(str)) == -1) {
    fprintf(stderr, "Could not write %s\n", kTraceMarkerPath);
  }

  if (close(fd) == -1) {
    fprintf(stderr, "Could not close %s\n", kTraceMarkerPath);
  }
}

void EnableEvent(const char* name) {
  std::string path = std::string(kTraceEventPath) + name + "/enable";
  if (!WriteToFile(path.c_str(), "1"))
    fprintf(stderr, "Could not enable event: %s", name);
}

void DisableEvent(const char* name) {
  std::string path = std::string(kTraceEventPath) + name + "/enable";
  if (!WriteToFile(path.c_str(), "0"))
    fprintf(stderr, "Could not disable event: %s", name);
}

ssize_t ReadPageFromRawPipe(int cpu, char* buffer) {
  constexpr const char* path_template =
      "/sys/kernel/debug/tracing/per_cpu/cpu%d/trace_pipe_raw";
  constexpr const size_t len = length(path_template) + 10;
  char path[len];
  snprintf(path, len, path_template, cpu);

  // TODO(hjd): Cache these fds?
  int fd = open(path, O_RDONLY | O_NONBLOCK);
  if (fd == -1) {
    fprintf(stderr, "Could not open %s\n", path);
    return -1;
  }

  // TODO(hjd): 4096
  ssize_t bytes_read = read(fd, buffer, 4096);
  if (bytes_read == -1 && errno == EAGAIN) {
    bytes_read = 0;
  }

  if (close(fd) == -1) {
    fprintf(stderr, "Could not close %s\n", path);
  }

  return bytes_read;
}

ssize_t GetNumberOfCpus() {
  return 144;
}

// The structure of a raw trace buffer page is as follows:
// First a page header:
//   8 bytes of timestamp
//   8 bytes of page length TODO(hjd): other fields also defined here?
//
// Some information about the layout of the page header is available in user
// space at: /sys/kernel/debug/tracing/events/header_event
void ParsePage(const char* ptr) {
  // TODO(hjd): Read this format dynamically?
  uint64_t timestamp = Read<uint64_t>(&ptr);
  uint64_t page_length = Read<uint64_t>(&ptr) & 0xfffful;
  CHECK(page_length <= 4080);
  const char* const start = ptr;
  const char* const end = ptr + page_length;

  // TODO(hjd): Remove.
  (void)start;
  (void)timestamp;

  while (ptr < end) {
    const uint32_t event_header = Read<uint32_t>(&ptr);
    const uint32_t type = event_header & 0x1f;
    const uint32_t time_delta = event_header >> 5;

    switch (type) {
      case kTypePadding: {
        // Left over page padding or discarded event.
        printf("Padding\n");
        if (time_delta == 0) {
          // TODO(hjd): Look at the next few bytes for read size;
        }
        CHECK(false);  // TODO(hjd): Handle
        break;
      }
      case kTypeTimeExtend: {
        // Extend the time delta.
        printf("Extended Time Delta\n");
        const uint32_t time_delta_ext = Read<uint32_t>(&ptr);
        (void)time_delta_ext;
        // TODO(hjd): Handle.
        break;
      }
      case kTypeTimeStamp: {
        // Sync time stamp with external clock.
        printf("Time Stamp\n");
        const uint64_t tv_nsec = Read<uint64_t>(&ptr);
        const uint64_t tv_sec = Read<uint64_t>(&ptr);
        // TODO(hjd): Handle.

        // TODO(hjd): Remove.
        (void)tv_nsec;
        (void)tv_sec;
        break;
      }
      default: {
        CHECK(type <= kTypeDataTypeLengthMax);
        // Where type is <=28 it represents the length of a data record
        if (type == 0) {
          CHECK(false);  // TODO(hjd): Look at the next few bytes for real size.
        }
        const char* next = ptr + 4 * type;

        uint16_t event_type = Read<uint16_t>(&ptr);

        // Common headers:
        // TODO(hjd): Read this format dynamically?
        Read<uint8_t>(&ptr);  // flags
        Read<uint8_t>(&ptr);  // preempt count
        uint32_t pid = Read<uint32_t>(&ptr);
        printf("Event type=%d pid=%d\n", event_type, pid);

        if (event_type == 5) {
          // Trace Marker Parser
          Read<uint64_t>(&ptr);  // ip
          const char* word_start = ptr;
          printf("  marker=%s", word_start);
          while (*ptr != '\0')
            ptr++;
        }

        // Jump to next event.
        ptr = next;
      }
    }
  }
}

}  // namespace perfetto
