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

#include <unistd.h>
#include <stdio.h>

#include "raw_trace_reader.h"

namespace perfetto {

ssize_t ReadRawPipe(int fd) {
  alignas(4096) char buf[4096];
  ssize_t rsize = read(fd, buf, sizeof(buf));
  if (rsize == -1)
    return -1;

  // TODO(hjd): Implement parsing here.
  return 0;
}

} // namespace perfetto

