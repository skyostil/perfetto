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

#include "protozero/scattered_stream_writer.h"

#include <string.h>

#include <algorithm>

#include "cpp_common/base.h"

namespace protozero {

ScatteredStreamWriter::Delegate::~Delegate() {}

ScatteredStreamWriter::ScatteredStreamWriter(Delegate* delegate)
    : delegate_(delegate),
      cur_range_({nullptr, nullptr}),
      write_ptr_(nullptr) {}

ScatteredStreamWriter::~ScatteredStreamWriter() {}

void ScatteredStreamWriter::Reset(ContiguousMemoryRange range) {
  cur_range_ = range;
  write_ptr_ = range.begin;
  DCHECK(write_ptr_ < cur_range_.end);
}

void ScatteredStreamWriter::Extend() {
  Reset(delegate_->GetNewBuffer());
}

void ScatteredStreamWriter::WriteByte(uint8_t value) {
  if (write_ptr_ >= cur_range_.end)
    Extend();
  *write_ptr_++ = value;
}

void ScatteredStreamWriter::WriteBytes(const uint8_t* src, size_t size) {
  uint8_t* const end = write_ptr_ + size;
  if (end <= cur_range_.end) {
    // Fast-path, the buffer fits into the current contiguous range.
    // TODO(primiano): perf optimization, this is a tracing hot path. The
    // compiler can make strong optimization on memcpy if the size arg is a
    // constexpr. Make a templated variant of this for fixed-size writes.
    memcpy(write_ptr_, src, size);
    write_ptr_ = end;
    return;
  }
  // Slow path, scatter the writes.
  size_t bytes_left = size;
  while (bytes_left > 0) {
    if (write_ptr_ >= cur_range_.end)
      Extend();
    const size_t burst_size = std::min(bytes_available(), bytes_left);
    WriteBytes(src, burst_size);
    bytes_left -= burst_size;
    src += burst_size;
  }
}

// TODO(primiano): perf optimization: I suspect that at the end this will always
// be called with |size| == 4, in which case we might just hardcode it.
ContiguousMemoryRange ScatteredStreamWriter::ReserveBytes(size_t size) {
  // Assume the reservations are always < kChunkSize.
  if (write_ptr_ + size > cur_range_.end) {
    Extend();
    DCHECK(write_ptr_ + size <= cur_range_.end);
  }
  uint8_t* begin = write_ptr_;
  write_ptr_ += size;
#ifndef NDEBUG
  memset(begin, '\xFF', size);
#endif
  return {begin, begin + size};
}

}  // namespace protozero
