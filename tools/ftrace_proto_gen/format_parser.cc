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

#include "tools/ftrace_proto_gen/format_parser.h"

namespace {

bool EatPrefix(const char** s, const char* end, const char* prefix) {
  int length = 0;
  for (int i=0; prefix[i]; i++) {
    if (*s + i == end)
      return false;
    if (prefix[i] != (*s)[i])
      return false;
    length++;
  }
  *s += length;
  return true;
}

bool EatWhitespace(const char** s, const char* end) {
  while (*s != end && (**s == ' ' || **s == '\t')) {
    (*s)++;
  }
  return true;
}

// e.g. "name: ion_alloc_buffer_end"
bool EatName(const char** s, const char* end, std::string& output) {
  char name[128];
  name[127] = '\0';
  int read = 0;
  int n = sscanf(*s, "name: %127[^\n]\n%n", name, &read);
  if (n != 1)
    return false;
  output = std::string(name);
  *s += read;
  return true;
}

// e.g. "ID: 143"
bool EatId(const char** s, const char* end, int* output) {
  int read = 0;
  int n = sscanf(*s, "ID: %d\n%n", output, &read);
  if (n != 1)
    return false;
  *s += read;
  return true;
}

// e.g. "format:"
bool EatFormatLine(const char** s, const char* end) {
  if (!EatPrefix(s, end, "format:\n"))
    return false;
  return true;
}

bool EatField(const char** s, const char* end, Field* output = nullptr) {
  int offset;
  int size;
  int is_signed_as_int;

  char type_and_name_buffer[128];
  type_and_name_buffer[127] = '\0';
  int read = 0;
  int n = sscanf(*s, "\tfield:%127[^;];\toffset: %d;\tsize: %d;\tsigned: %d;\n%n", type_and_name_buffer, &offset, &size, &is_signed_as_int, &read);
  if (n != 4)
    return false;

  *s += read;

  if (!output)
    return true;

  output->type_and_name = std::string(type_and_name_buffer);
  output->offset = offset;
  output->size = size;
  output->is_signed = is_signed_as_int != 0;

  return true;
}

} // namespace

bool ParseFormat(const std::string& input, Format* output) {
  return ParseFormat(input.c_str(), input.length(), output);
}

bool ParseFormat(const char* s, size_t len, Format* output) {
  const char* const end = s + len;
  std::string name;
  int id;
  std::string fmt;
  std::vector<Field> fields;

  if (!EatName(&s, end, name))
    return false;
  if (!EatId(&s, end, &id))
    return false;
  if (!EatFormatLine(&s, end))
    return false;

  // Common fields:
  for (int i=0; i<4; i++) {
    if (!EatField(&s, end))
      return false;
  }

  // Intresting fields:
  while (s[0] != 'p') {
    Field field;
    if (!EatField(&s, end, &field))
      return false;
    fields.push_back(field);
  }

  if (!EatPrefix(&s, end, "print fmt:"))
    return false;
  if (!EatWhitespace(&s, end))
    return false;

  fmt = std::string(s, end);
  s = end;

  if (!output)
    return true;

  output->name = name;
  output->id = id;
  output->fmt = fmt;
  output->fields = std::move(fields);

  return true;
}

