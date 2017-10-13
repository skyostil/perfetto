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

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>

#include <memory>

#include "tools/ftrace_proto_gen/format_parser.h"
#include "tools/ftrace_proto_gen/proto_writer.h"

int main(int argc, const char** argv) {
  if (argc != 3) {
    printf("Usage: ./ftrace_proto_gen in.format out.proto\n");
    exit(1);
  }

  const char* input_path = argv[1];
  const char* output_path = argv[2];

  int fin = open(input_path, O_RDONLY);
  if (fin < 0) { 
    fprintf(stderr, "Failed to open %s\n", input_path);
    return 1;
  }
  off_t fsize = lseek(fin, 0, SEEK_END);
  std::unique_ptr<char[]> buf(new char[fsize + 1]);
  ssize_t rsize;
  do {
    lseek(fin, 0, SEEK_SET);
    rsize = read(fin, buf.get(), static_cast<size_t>(fsize));
  } while(rsize < 0 && errno == EINTR);
  // TODO(hjd): CHECK(rsize == fsize);
  size_t length = static_cast<size_t>(rsize);
  buf[length] = '\0';
  close(fin);

  perfetto::FtraceEvent format;
  if (!perfetto::ParseFtraceEvent(buf.get(), length, &format)) {
    fprintf(stderr, "Could not parse file %s.\n", input_path);
    exit(1);
  }

  perfetto::Proto proto;
  if (!perfetto::GenerateProto(format, &proto))
    exit(1);

  FILE *fout = fopen(output_path, "w");
  if (fout == nullptr) {
    perror("Error");
    exit(1);
  }

  perfetto::WriteProto(fout, proto);
}

