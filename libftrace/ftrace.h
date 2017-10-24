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

#ifndef LIBFTRACE_FTRACE_H_
#define LIBFTRACE_FTRACE_H_

#include <unistd.h>

namespace perfetto {

// TODO(hjd): Read this at run time?
const size_t kPageSize = 4096;

// Clears the trace buffers for all CPUs. Blocks until this is done.
void ClearTrace();

// Writes the string |str| as an event into the trace buffer.
void WriteTraceMarker(const char* str);

// Perform a non-blocking of read of trace_pipe_raw for |cpu|.
// Data is written into |buffer|. |buffer| should be at least one page
// Returns number of bytes read, 0 if no bytes were read or reading would block
// and -1 on an error.
ssize_t ReadPageFromRawPipe(int cpu, char* buffer);

// Parse a raw page and print some facts about it.
void ParsePage(const char* buffer);

// Enable the event |name|.
void EnableEvent(const char* name);

// Disable the event |name|.
void DisableEvent(const char* name);

// Returns the number of CPUs or -1 on an error.
// This should match the number of tracing/per_cpu/cpuXX directories. If
// GetNumberOfCpus() returns 4 we should see the directories:
// cpu0, cpu1, cpu2, cpu3.
ssize_t GetNumberOfCpus();

} // namespace perfetto

#endif  // LIBFTRACE_FTRACE_H_
