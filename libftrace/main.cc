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

#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <memory>

#include "libftrace/ftrace.h"

int main(int argc, const char** argv) {
  std::unique_ptr<char[]> buffer =
      std::unique_ptr<char[]>(new char[perfetto::kPageSize]);

  perfetto::ClearTrace();

  perfetto::WriteTraceMarker("Hello, world!");

  for (int i = 1; i < argc; i++) {
    printf("Enabling: %s\n", argv[i]);
    perfetto::EnableEvent(argv[i]);
  }

  // Sleep for one second so we get some events
  sleep(1);

  for (int i = 0; i < perfetto::GetNumberOfCpus(); i++) {
    ssize_t bytes_read = perfetto::ReadPageFromRawPipe(i, buffer.get());
    if (bytes_read <= 0) {
      continue;
    }
    printf("=== Data for cpu %d ===\n", i);
    perfetto::ParsePage(buffer.get());
    printf("=======================\n\n");
  }

  for (int i = 1; i < argc; i++) {
    printf("Disable: %s\n", argv[i]);
    perfetto::DisableEvent(argv[i]);
  }

  return 0;
}
