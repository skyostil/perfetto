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

#include "ftrace_to_proto_translation_table.h"

#include <fstream>
#include <sstream>
#include <string>

#include "ftrace_reader/format_parser.h"
#include "ftrace_reader/ftrace_to_proto.h"

namespace perfetto {

namespace {

#define MAX_FIELD_LENGTH 127
#define STRINGIFY(x) STRINGIFY2(x)
#define STRINGIFY2(x) #x

std::string ReadFileIntoString(std::string path) {
  std::ifstream fin(path, std::ios::in);
  if (!fin) {
    return "";
  }
  std::ostringstream stream;
  stream << fin.rdbuf();
  fin.close();
  return stream.str();
}

}  // namespace

// static
std::unique_ptr<FtraceToProtoTranslationTable>
FtraceToProtoTranslationTable::Create(std::string path_to_root) {
  PERFETTO_CHECK(path_to_root[path_to_root.length() - 1] == '/');
  std::map<size_t, Event> id_to_events;
  std::vector<Field> common_fields;

  std::vector<Event> events;
  std::string available_path = path_to_root + "/available_events";
  std::string available_contents = ReadFileIntoString(available_path);
  PERFETTO_CHECK(available_contents != "");
  {
    std::unique_ptr<char[], base::FreeDeleter> copy(
        strdup(available_contents.c_str()));
    char group_buffer[MAX_FIELD_LENGTH + 1];
    char name_buffer[MAX_FIELD_LENGTH + 1];
    char* s = copy.get();
    for (char* line = strtok(s, "\n"); line; line = strtok(nullptr, "\n")) {
      if (sscanf(line,
                 "%" STRINGIFY(MAX_FIELD_LENGTH) "[^:]:%" STRINGIFY(
                     MAX_FIELD_LENGTH) "s",
                 group_buffer, name_buffer) == 2) {
        std::string name = std::string(name_buffer);
        std::string group = std::string(group_buffer);
        events.emplace_back(Event{name, group});
      }
    }
  }

  for (Event e : events) {
    std::string path =
        path_to_root + "/events/" + e.group + "/" + e.name + "/format";
    std::string contents = ReadFileIntoString(path);
    PERFETTO_CHECK(contents != "");
    FtraceEvent fe;
    if (!ParseFtraceEvent(contents, &fe))
      continue;
    e.ftrace_event_id = fe.id;
    e.fields.reserve(fe.fields.size());
    for (FtraceEvent::Field fe_field : fe.fields) {
      e.fields.push_back(Field{fe_field.offset, fe_field.size});
    }

    id_to_events[e.ftrace_event_id] = e;
  }

  auto table = std::unique_ptr<FtraceToProtoTranslationTable>(
      new FtraceToProtoTranslationTable(std::move(id_to_events),
                                        std::move(common_fields)));
  return table;
}

FtraceToProtoTranslationTable::FtraceToProtoTranslationTable(
    std::map<size_t, Event> events,
    std::vector<Field> common_fields)
    : events_(std::move(events)), common_fields_(std::move(common_fields)) {}

FtraceToProtoTranslationTable::~FtraceToProtoTranslationTable() = default;

}  // namespace perfetto
