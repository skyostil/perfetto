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

bool EatNewline(const char** s, const char* end) {
  return EatPrefix(s, end, "\n");
}

bool EatSemicolon(const char** s, const char* end) {
  return EatPrefix(s, end, ";");
}

bool EatWhitespace(const char** s, const char* end) {
  while (*s != end && (**s == ' ' || **s == '\t')) {
    (*s)++;
  }
  return true;
}

bool EatWord(const char** s, const char* end, std::string& output) {
  const char* const start = *s;
  size_t length = 0;
  while (*s != end && **s != ' ' && **s != ';' && **s != '\n') {
    (*s)++;
    length++;
  }
  if (length == 0)
    return false;
  output = std::string(start, length);
  return true;
}

bool EatToSemicolon(const char** s, const char* end, std::string& output) {
  const char* const start = *s;
  size_t length = 0;
  while (*s != end && **s != ';') {
    (*s)++;
    length++;
  }
  if (length == 0)
    return false;
  output = std::string(start, length);
  return true;
}

bool EatInt(const char** s, const char* end, int* output) {
  std::string number;
  if (!EatWord(s, end, number))
    return false;

  const char* ptr = number.c_str();
  int n = static_cast<int>(std::strtol(ptr, const_cast<char**>(&ptr), 10));
  if (number.c_str() == ptr)
    return false;

  *output = n;
  return true;
}

bool EatBool(const char** s, const char* end, bool* output) {
  int number;
  if (!EatInt(s, end, &number))
    return false;
  *output = number != 0;
  return true;
}

// e.g. "name: ion_alloc_buffer_end"
bool EatName(const char** s, const char* end, std::string& output) {
  if (!EatPrefix(s, end, "name:"))
    return false;
  if (!EatWhitespace(s, end))
    return false;
  if (!EatWord(s, end, output))
      return false;
  if (!EatNewline(s, end))
    return false;
  return true;
}

// e.g. "ID: 143"
bool EatId(const char** s, const char* end, int* output) {
  if (!EatPrefix(s, end, "ID:"))
    return false;
  if (!EatWhitespace(s, end))
    return false;
  if (!EatInt(s, end, output))
      return false;
  if (!EatNewline(s, end))
    return false;
  return true;
}

// e.g. "format:"
bool EatFormatLine(const char** s, const char* end) {
  if (!EatPrefix(s, end, "format:"))
    return false;
  if (!EatNewline(s, end))
    return false;
  return true;
}

bool EatFieldPreamble(const char** s, const char* end) {
  if (!EatWhitespace(s, end))
    return false;
  if (!EatPrefix(s, end, "field:"))
    return false;
  return true;
}

bool EatFieldContents(const char** s, const char* end, Field* output = nullptr) {
  std::string type_and_name;
  int offset;
  int size;
  bool is_signed;

  if (!EatToSemicolon(s, end, type_and_name))
    return false;
  if (!EatSemicolon(s, end))
    return false;

  if (!EatWhitespace(s, end))
    return false;
  if (!EatPrefix(s, end, "offset:"))
    return false;
  if (!EatInt(s, end, &offset))
    return false;
  if (!EatSemicolon(s, end))
    return false;

  if (!EatWhitespace(s, end))
    return false;
  if (!EatPrefix(s, end, "size:"))
    return false;
  if (!EatInt(s, end, &size))
    return false;
  if (!EatSemicolon(s, end))
    return false;

  if (!EatWhitespace(s, end))
    return false;
  if (!EatPrefix(s, end, "signed:"))
    return false;
  if (!EatBool(s, end, &is_signed))
    return false;
  if (!EatSemicolon(s, end))
    return false;

  if (!EatNewline(s, end))
    return false;

  if (output == nullptr)
    return true;

  output->type_and_name = type_and_name;
  output->offset = offset;
  output->size = size;
  output->is_signed = is_signed;

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
    if (!EatFieldPreamble(&s, end))
      return false;
    if (!EatFieldContents(&s, end))
      return false;
  }

  if (!EatNewline(&s, end))
    return false;

  // Intresting fields:
  for (;;) {
    Field field;
    if (!EatFieldPreamble(&s, end))
      break;
    if (!EatFieldContents(&s, end, &field))
      return false;
    fields.push_back(field);
  }

  if (!EatNewline(&s, end))
    return false;
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

