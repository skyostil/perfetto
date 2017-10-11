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

#include "libtracing/src/unix_rpc/unix_shared_memory.h"

#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#include <memory>
#include <utility>

extern "C" {
const unsigned int MFD_CLOEXEC = 0;
const unsigned int MFD_ALLOW_SEALING = 0;
int memfd_create(const char* name, unsigned int flags);
int memfd_create(const char* name, unsigned int flags) {
  return -1;
}
}

namespace perfetto {

std::unique_ptr<UnixSharedMemory> UnixSharedMemory::Create(size_t size) {
  int fd = memfd_create("blah", MFD_CLOEXEC | MFD_ALLOW_SEALING);
  void* start = mmap(0, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (start == MAP_FAILED)
    return nullptr;
  return std::unique_ptr<UnixSharedMemory>(
      new UnixSharedMemory(start, size, fd));
}

UnixSharedMemory::UnixSharedMemory(void* start, size_t size, int fd)
    : start_(start), size_(size), fd_(fd) {}

UnixSharedMemory::~UnixSharedMemory() {
  munmap(start(), size());
  close(fd_);
}

}  // namespace perfetto
