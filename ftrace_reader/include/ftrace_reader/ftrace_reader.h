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

#ifndef FTRACE_READER_FTRACE_READER_H_
#define FTRACE_READER_FTRACE_READER_H_

#include <memory>

#include "ftrace_reader/ftrace_controller.h"

namespace perfetto {

class FtracePaths;

// Root of the ftrace_reader API.
// When initialized it reads:
//  available_events    - to figure out which events exist
//  events/header_event - as a sanitiy check
//  events/page_header  - as a snaitiy check
//  events/*/*/format   - to get the format of the common and non-common fields
// and uses this data creates the configuration the FtraceCpuReaders uses to
// parse the raw ftrace format.
//
// FtraceReader owns each FtraceCpuReader. Users call |GetCpuReader(int cpu)|
// to access the reader for a specific CPU.
//
// TODO(hjd): Implement the above.
class FtraceReader {
 public:
  FtraceReader();
  ~FtraceReader();
  FtraceController* GetController();

 private:
  FtraceReader(const FtraceReader&) = delete;
  FtraceReader& operator=(const FtraceReader&) = delete;

  std::unique_ptr<FtracePaths> paths_;
  FtraceController controller_;
};

}  // namespace perfetto

#endif  // FTRACE_READER_FTRACE_READER_H_
