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

#include "format_parser/format_parser.h"
#include <stdio.h>
#define MAXBUFLEN 1000000

int main(int argc, const char** argv) {
  if (argc != 2) {
    printf("Usage: ./format_parser format.txt.\n");
    exit(1);
  }

  char source[MAXBUFLEN + 1];
  FILE *fp = fopen(argv[1], "r");
  size_t length = 0;
  if (fp != NULL) {
    length = fread(source, sizeof(char), MAXBUFLEN, fp);
    if (ferror(fp) != 0) {
      fputs("Error reading file", stderr);
    } else {
      source[length++] = '\0';
    }
    fclose(fp);
  }

  Format format;
  if (!ParseFormat(source, length, &format)) {
    printf("Format file invalid.\n");
    exit(1);
  }

  printf("Parsed event!\n");
  printf("    id: %d\n", format.id);
  printf("  name: %s\n", format.name.c_str());
  printf("fields:\n");
  printf("    %25s %7s %4s %7s\n", "type", "offset", "size", "signed?");
  for (const Field& field : format.fields) {
    printf("    %25s %7d %4d %7s\n",
        field.type_and_name.c_str(),
        field.offset,
        field.size,
        field.is_signed ? "yes" : "no");
  }

}
