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

#ifndef TOOLS_FTRACE_PROTO_GEN_FTRACE_GEN_H_
#define TOOLS_FTRACE_PROTO_GEN_FTRACE_GEN_H_

#include <stdint.h>

#include <iosfwd>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

namespace perfetto {

struct FtraceEventField {
  std::string type_and_name;
  int offset;
  int size;
  bool is_signed;

  bool operator==(const FtraceEventField& other) const {
    return std::tie(type_and_name, offset, size, is_signed) ==
           std::tie(other.type_and_name, other.offset, other.size,
                    other.is_signed);
  }
};


struct FtraceEvent {
  using Field = FtraceEventField;
  std::string name;
  int id;
  std::vector<FtraceEventField> fields;
};


struct ProtoField {
  std::string type;
  std::string name;
  uint32_t number;
};

struct Proto {
  using Field = ProtoField;
  std::string name;
  std::vector<ProtoField> fields;

  std::string ToString();
};

FtraceEvent Test();

bool GenerateProto(const FtraceEvent& format, Proto* proto_out);
std::string InferProtoType(const FtraceEventField& field);
std::string GetNameFromTypeAndName(const std::string& type_and_name);

// Allow gtest to pretty print FtraceEventField.
// ::std::ostream& operator<<(::std::ostream& os, const FtraceEventField&);
// void PrintTo(const FtraceEventField& args, ::std::ostream* os);

}  // namespace perfetto

#endif  // TOOLS_FTRACE_PROTO_GEN_FTRACE_GEN_H_
