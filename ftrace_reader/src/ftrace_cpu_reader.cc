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

#include "ftrace_event_bundle.pbzero.h"
#include "ftrace_to_proto_translation_table.h"

namespace perfetto {

FtraceCpuReader::FtraceCpuReader(const FtraceToProtoTranslationTable* table,
                                 size_t cpu,
                                 base::ScopedFile fd)
    : table_(table), cpu_(cpu), fd_(std::move(fd)) {}

void FtraceCpuReader::Read(const Config&, pbzero::FtraceEventBundle*) {}

FtraceCpuReader::~FtraceCpuReader() = default;
FtraceCpuReader::FtraceCpuReader(FtraceCpuReader&&) = default;

FtraceCpuReader::Config FtraceCpuReader::CreateConfig(
    std::set<std::string> event_names) {
  std::vector<bool> enabled(table_->largest_id());
  for (const std::string& name : event_names) {
    const FtraceToProtoTranslationTable::Event* event =
        table_->GetEventByName(name);
    if (!event)
      continue;
    enabled[event->ftrace_event_id - 1] = true;
  }
  return Config(std::move(enabled));
}

FtraceCpuReader::Config::~Config() = default;
FtraceCpuReader::Config::Config(Config&&) = default;

FtraceCpuReader::Config::Config(std::vector<bool> enabled)
    : enabled_(enabled){};
bool FtraceCpuReader::Config::IsEnabled(size_t ftrace_event_id) const {
  PERFETTO_DCHECK(ftrace_event_id < enabled_.size());
  // FtraceEventIds are 1-indexed.
  return enabled_[ftrace_event_id - 1];
}

}  // namespace perfetto
