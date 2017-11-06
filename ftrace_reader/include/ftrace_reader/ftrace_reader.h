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

class FtraceReader {
 public:
  FtraceReader();
  FtraceController* GetController();

 private:
  FtraceReader(const FtraceReader&) = delete;
  FtraceReader& operator=(const FtraceReader&) = delete;

  std::unique_ptr<FtracePaths> paths_;
  FtraceController controller_;
};

}  // namespace perfetto

#endif  // FTRACE_READER_FTRACE_READER_H_
