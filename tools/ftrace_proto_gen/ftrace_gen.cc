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

#include "tools/ftrace_proto_gen/ftrace_gen.h"

#include <string>
#include <vector>
#include <set>

namespace perfetto {
namespace {

bool IsCIdentifier(const std::string& s) {
  for (const char c : s) {
    if (!(std::isalnum(c) || c == '_'))
      return false;
  }
  return s.size() > 0 && !std::isdigit(s[0]);
}

} // namespace

std::string NameFromTypeAndName(std::string type_and_name) {
  size_t right = type_and_name.size();
  if (right == 0)
    return "";

  if (type_and_name[type_and_name.size()-1] == ']') {
    right = type_and_name.rfind('[');
    if (right == std::string::npos)
      return "";
  }

  size_t left = type_and_name.rfind(' ', right);
  if (left == std::string::npos)
    return "";
  left++;

  std::string result = type_and_name.substr(left, right-left);
  if (!IsCIdentifier(result))
    return "";

  return result;
}

std::string InferProtoType(const FtraceEvent::Field& field) {
  // Very scientific:
  if (field.type_and_name.find("char *") != std::string::npos)
    return "string";
  if (field.size <= 4 && field.is_signed)
    return "int32";
  if (field.size <= 4 && !field.is_signed)
    return "uint32";
  if (field.size <= 8 && field.is_signed)
    return "int64";
  if (field.size <= 8 && !field.is_signed)
    return "uint64";
  return "";
}

bool GenerateProto(const FtraceEvent& format, Proto* proto_out) {
  proto_out->name = format.name;
  proto_out->fields.reserve(format.fields.size());
  std::set<std::string> seen;
  uint32_t i = 1;
  for (const FtraceEvent::Field& field : format.fields) {
    std::string name = NameFromTypeAndName(field.type_and_name);
    // TODO(hjd): Handle dup names.
    if (name == "" || seen.count(name))
      continue;
    seen.insert(name);
    std::string type = InferProtoType(field);
    // Check we managed to infer a type.
    if (type == "")
      continue;
    proto_out->fields.emplace_back(Proto::Field{type, name, i});
    i++;
  }

  return true;
}

} // namespace perfetto

