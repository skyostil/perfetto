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

#ifndef LIBTRACING_INCLUDE_LIBTRACING_CORE_SHARED_MEMORY_H_
#define LIBTRACING_INCLUDE_LIBTRACING_CORE_SHARED_MEMORY_H_

#include <stddef.h>

namespace perfetto {

class SharedMemory {
 public:
  // The embedder is expected to tear down the resource associated to this
  // Shared memory region when this class is destroyed.
  virtual ~SharedMemory() {}

  virtual void* start() const = 0;
  virtual size_t size() const = 0;
};

}  // namespace perfetto

#endif  // LIBTRACING_INCLUDE_LIBTRACING_CORE_SHARED_MEMORY_H_
