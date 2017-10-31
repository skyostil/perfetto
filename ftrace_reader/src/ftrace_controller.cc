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

#include "ftrace_reader/ftrace_controller.h"

#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <string>

#include "cpp_common/base.h"

namespace perfetto {

namespace {

// TODO(b/68242551): Do not hardcode these paths.
// This directory contains the 'format' and 'enable' files for each event.
// These are nested like so: group_name/event_name/{format, enable}
const char kTraceEventPath[] = "/sys/kernel/debug/tracing/events/";

// Reading this file produces human readable trace output.
// Writing to this file clears all trace buffers for all CPUS.
const char kTracePath[] = "/sys/kernel/debug/tracing/trace";

// Writing to this file injects an event into the trace buffer.
const char kTraceMarkerPath[] = "/sys/kernel/debug/tracing/trace_marker";

bool WriteToFile(const char* path, const char* str) {
  ScopedFile fd(open(path, O_WRONLY));
  if (fd.get() == -1)
    return false;
  HANDLE_EINTR(write(fd.get(), str, strlen(str)));
  ssize_t result = HANDLE_EINTR(write(fd.get(), str, strlen(str)));
  DCHECK(result != -1);
  return result != -1;
}

}  // namespace

FtraceController::FtraceController() {
}

void FtraceController::ClearTrace() {
  ScopedFile fd(open(kTracePath, O_WRONLY | O_TRUNC));
  if (fd.get() == -1) {
    DCHECK(false); // Could not clear.
    return;
  }
}

void FtraceController::WriteTraceMarker(const char* str) {
  ScopedFile fd(open(kTraceMarkerPath, O_WRONLY));
  if (fd.get() == -1) {
    DCHECK(false); // Could not open.
    return;
  }

  ssize_t result = HANDLE_EINTR(write(fd.get(), str, strlen(str)));
  DCHECK(result != -1);
}

void FtraceController::EnableEvent(const char* name) {
  std::string path = std::string(kTraceEventPath) + name + "/enable";
  bool success = WriteToFile(path.c_str(), "1");
  DCHECK(success);
}

void FtraceController::DisableEvent(const char* name) {
  std::string path = std::string(kTraceEventPath) + name + "/enable";
  bool success = WriteToFile(path.c_str(), "0");
  DCHECK(success);
}

}  // namespace perfetto
