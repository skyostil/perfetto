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
#ifndef FTRACE_READER_FTRACE_PATHS_H_
#define FTRACE_READER_FTRACE_PATHS_H_

#include <string>

namespace perfetto {

class FtracePaths {
 public:
  FtracePaths(const std::string& root);

  // Writing to this file injects an event into the trace buffer.
  std::string trace_marker() const { return root_ + "trace_marker"; }

  // Reading this file produces human readable trace output.
  // Writing to this file clears all trace buffers for all CPUS.
  std::string trace() const { return root_ + "trace"; }

  // Reading this file returns 1/0 if tracing is enabled/disabled.
  // Writing 1/0 to this file enables/disables tracing.
  // Disabling tracing with this file prevents further writes but
  // does not clear the buffer.
  std::string tracing_on() const { return root_ + "tracing_on"; }

  // This file contains all the events, one per line in the format:
  // GROUP:NAME
  std::string available_events() const { return root_ + "available_events"; }

  // The events/ directory contains the 'format' and 'enable' files
  // for each event.
  // These are nested like so: group_name/event_name/{format, enable}
  std::string Enable(const std::string& group, const std::string& name) const {
    return root_ + "events/" + group + "/" + name + "/enable";
  }

  std::string Format(const std::string& group, const std::string& name) const {
    return root_ + "events/" + group + "/" + name + "/format";
  }

  std::string TracePipeRaw(size_t cpu) const {
    return root_ + "per_cpu/" + std::to_string(cpu) + "/trace_pipe_raw";
  }

  FtracePaths(FtracePaths&&) = default;

 private:
  const std::string root_;

  FtracePaths(const FtracePaths&) = delete;
  FtracePaths& operator=(const FtracePaths&) = delete;
};

}  // namespace perfetto

#endif  // FTRACE_READER_FTRACE_PATHS_H_
