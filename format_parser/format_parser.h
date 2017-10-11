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

#ifndef FORMAT_PARSER_FORMAT_PARSER_H_
#define FORMAT_PARSER_FORMAT_PARSER_H_

#include <stddef.h>
#include <string>
#include <vector>

struct Field {
  std::string type_and_name;
  int offset;
  int size;
  bool is_signed;
};

struct Format {
  std::string name;
  int id;
  std::string fmt;
  std::vector<Field> fields;
};

bool ParseFormat(const char* s, size_t len, Format* output = nullptr);

#endif // FORMAT_PARSER_FORMAT_PARSER_H_
