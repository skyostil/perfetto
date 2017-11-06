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

#include "ipc/src/buffered_frame_deserializer.h"

#include <inttypes.h>
#include <sys/mman.h>

#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/utils.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "wire_protocol.pb.h"  // protobuf generated header.

namespace perfetto {
namespace ipc {

namespace {
constexpr size_t kPageSize = 4096;
constexpr size_t kGuardRegionSize = kPageSize;
}  // namespace

BufferedFrameDeserializer::BufferedFrameDeserializer(size_t max_capacity)
    : capacity_(max_capacity) {
  PERFETTO_CHECK(max_capacity % kPageSize == 0);
}

BufferedFrameDeserializer::~BufferedFrameDeserializer() {
  if (!buf_)
    return;
  int res = munmap(buf_, capacity_ + kGuardRegionSize);
  PERFETTO_DCHECK(res == 0);
}

std::pair<char*, size_t> BufferedFrameDeserializer::BeginRecv() {
  // Upon the first recv initialize the buffer to the max message size but
  // release the physical memory for all but the first page. The kernel will
  // automatically give us physical pages back as soon as we page-fault on them.
  // Also add a guard page after the buffer as a safety net against overflows.
  if (!buf_) {
    PERFETTO_DCHECK(size_ == 0);
    buf_ = reinterpret_cast<char*>(mmap(nullptr, capacity_ + kGuardRegionSize,
                                        PROT_READ | PROT_WRITE,
                                        MAP_ANONYMOUS | MAP_PRIVATE, 0, 0));
    PERFETTO_CHECK(buf_ != MAP_FAILED);

    // Surely we are going to use at least the first page. There is very little
    // point in madvising that as well and immedately after telling the kernel
    // that we want it back (via recv()).
    int res = madvise(buf_ + kPageSize,
                      capacity_ + kGuardRegionSize - kPageSize, MADV_DONTNEED);
    PERFETTO_DCHECK(res == 0);

    res = mprotect(buf_ + capacity_, kGuardRegionSize, PROT_NONE);
    PERFETTO_DCHECK(res == 0);
  }

  PERFETTO_CHECK(capacity_ > size_);
  return std::make_pair(buf_ + size_, capacity_ - size_);
}

bool BufferedFrameDeserializer::EndRecv(size_t recv_size) {
  PERFETTO_CHECK(recv_size + size_ <= capacity_);
  size_ += recv_size;

  // At this point the contents buf_ can contain:
  // A) Only a fragment of the header (the size of the frame). E.g.,
  //    00 00 00 (the header is 4 bytes, one is missing).
  //
  // B) A header and a part of the frame. E.g.,
  //     00 00 00 05         11 22 33
  //    [ header, size=5 ]  [ Partial frame ]
  //
  // C) One or more complete header+frame. E.g.,
  //     00 00 00 05         11 22 33 44 55   00 00 00 03        AA BB CC
  //    [ header, size=5 ]  [ Whole frame ]  [ header, size=3 ] [ Whole frame ]
  //
  // D) Some complete header+frame(s) and a partial header or frame (C + A/B).
  //
  // C Is the more likely case and the one we are optimizing for. A, B, D can
  // happen because of the streaming nature of the socket. Realistically they
  // will happen whenever a frame > kMinRecvBuffer is sent over.
  // The invariant of this function is that, when it returns, buf_ is either
  // empty (we drained all the complete frames) or starts with the header of the
  // next, still incomplete, frame.

  // The header is just the number of bytes of the Frame protobuf message.
  const size_t kHeaderSize = sizeof(uint32_t);

  size_t consumed_size = 0;
  for (;;) {
    if (size_ < consumed_size + kHeaderSize)
      break;  // Case A, not enough data to read even the header.

    // Read the header into |payload_size|.
    uint32_t payload_size = 0;
    const char* rd_ptr = buf_ + consumed_size;
    memcpy(base::AssumeLittleEndian(&payload_size), rd_ptr, kHeaderSize);
    const size_t next_frame_size = kHeaderSize + payload_size;
    rd_ptr += kHeaderSize;

    if (size_ < consumed_size + next_frame_size) {
      // Case B. We got the header but not the whole frame.
      if (next_frame_size > capacity_) {
        // The caller is expected to shut down the socket and give up at this
        // point. If it doesn't do that and insists going on at some point it
        // will hit the capacity check in BeginRecv().
        PERFETTO_DLOG("Frame too large (size %zu)", next_frame_size);
        return false;
      }
      break;
    }

    // Case C. We got at least one header and whole frame.
    DecodeFrame(rd_ptr, payload_size);
    consumed_size += next_frame_size;
  }

  PERFETTO_DCHECK(consumed_size <= size_);
  if (consumed_size > 0) {
    // Shift out the consumed data from the buffer. In the typical case (C)
    // there is nothig to shift really, just setting size_ = 0 is enough.
    // Shifting is only for the (unlikely) case D.
    size_ -= consumed_size;
    if (size_ > 0) {
      // Case D. We consumed some frames but there is a leftover at the end of
      // the buffer. Shift out the consumed bytes, so that on the next round
      // |buf_| starts with the header of the next unconsumed frame.
      const char* move_begin = buf_ + consumed_size;
      PERFETTO_CHECK(move_begin > buf_);
      PERFETTO_CHECK(move_begin + size_ <= buf_ + capacity_);
      memmove(buf_, move_begin, size_);
    }
    // If we just finished decoding a large frame that used more than one page,
    // release the extra memory in the buffer. Large frames should be quite
    // rare.
    if (consumed_size > kPageSize) {
      size_t size_rounded_up = (size_ / kPageSize + 1) * kPageSize;
      if (size_rounded_up < capacity_) {
        char* madvise_begin = buf_ + size_rounded_up;
        const size_t madvise_size = capacity_ - size_rounded_up;
        PERFETTO_CHECK(madvise_begin > buf_ + size_);
        PERFETTO_CHECK(madvise_begin + madvise_size <= buf_ + capacity_);
        int res = madvise(madvise_begin, madvise_size, MADV_DONTNEED);
        PERFETTO_DCHECK(res == 0);
      }
    }
  }
  // At this point |size_| == 0 for case C, > 0 for cases A, B, D.
  return true;
}

std::unique_ptr<Frame> BufferedFrameDeserializer::PopNextFrame() {
  if (decoded_frames_.empty())
    return nullptr;
  std::unique_ptr<Frame> frame = std::move(decoded_frames_.front());
  decoded_frames_.pop_front();
  return frame;
}

void BufferedFrameDeserializer::DecodeFrame(const char* data, size_t size) {
  std::unique_ptr<Frame> frame(new Frame);
  const int sz = static_cast<int>(size);
  ::google::protobuf::io::ArrayInputStream stream(data, sz);
  if (frame->ParseFromBoundedZeroCopyStream(&stream, sz))
    decoded_frames_.push_back(std::move(frame));
}

}  // namespace ipc
}  // namespace perfetto
