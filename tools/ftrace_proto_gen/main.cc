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
#include "tools/ftrace_proto_gen/proto_writer.h"

#include <stdio.h>
#define MAXBUFLEN 1000000

namespace {

} // namespace

int main(int argc, const char** argv) {
  if (argc != 3) {
    printf("Usage: ./ftrace_proto_gen in.format out.proto\n");
    exit(1);
  }

  char source[MAXBUFLEN + 1];
  FILE* fin = fopen(argv[1], "r");
  if (fin == nullptr) {
    perror("Error");
    exit(1);
  }

  size_t length = 0;
  if (fin != NULL) {
    length = fread(source, sizeof(char), MAXBUFLEN, fin);
    if (ferror(fin) != 0) {
      fprintf(stderr, "Error reading file %s\n", argv[1]);
    } else {
      source[length++] = '\0';
    }
    fclose(fin);
  }

  Format format;
  if (!ParseFormat(source, length, &format)) {
    printf("Format file invalid.\n");
    exit(1);
  }

  
  Proto proto;
  if (!GenerateProto(format, &proto))
    exit(1);

  FILE *fout = fopen(argv[2], "w");
  if (fout == nullptr) {
    perror("Error");
    exit(1);
  }

  WriteProto(fout, proto);
}
