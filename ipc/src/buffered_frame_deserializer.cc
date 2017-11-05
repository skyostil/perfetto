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

#include <type_traits>
#include <utility>

#include "base/logging.h"
#include "base/utils.h"
#include "google/protobuf/io/zero_copy_stream_impl_lite.h"

#include "wire_protocol.pb.h"  // protobuf generated header.

namespace perfetto {
namespace ipc {

// static
constexpr size_t BufferedFrameDeserializer::kMinRecvBuffer;
constexpr size_t BufferedFrameDeserializer::kMaxFrameSize;

BufferedFrameDeserializer::BufferedFrameDeserializer() = default;

std::pair<char*, size_t> BufferedFrameDeserializer::BeginRecv() {
  if (!buf_) {
    buf_.reset(reinterpret_cast<char*>(malloc(kMinRecvBuffer)));
    capacity_ = kMinRecvBuffer;
    size_ = 0;
  }
  static_assert(kMinRecvBuffer > 0, "kMinRecvBuffer must be > 0");
  PERFETTO_CHECK(capacity_ >= size_ + kMinRecvBuffer);
  return std::make_pair(buf_.get() + size_, capacity_ - size_);
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

  size_t next_frame_size = 0;
  size_t consumed_size = 0;
  const char* const begin = buf_.get();
  const char* const end = buf_.get() + size_;
  const char* rd_ptr = begin;
  for (;;) {
    if (rd_ptr + kHeaderSize > end)
      break;  // Case A, not enough data to read even the header.

    // Read the header into (payload_size, next_frame_size).
    uint32_t payload_size = 0;
    memcpy(base::AssumeLittleEndian(&payload_size), rd_ptr, kHeaderSize);
    rd_ptr += kHeaderSize;

    if (rd_ptr + payload_size > end) {
      // Case B. We got the header but not the whole frame.
      next_frame_size = kHeaderSize + payload_size;
      if (next_frame_size > kMaxFrameSize) {
        PERFETTO_DLOG("Frame too large (size %zu)", next_frame_size);
        return false;  // TODO invalidate so we hit CHECKs.
      }
      break;
    }

    // Case C. We got at least the header and the whole frame.
    DecodeFrame(rd_ptr, payload_size);
    consumed_size += kHeaderSize + payload_size;
    rd_ptr += payload_size;
  }

  // Finally, shift out the consumed data from the buffer.
  PERFETTO_DCHECK(consumed_size <= size_);
  size_ -= consumed_size;
  if (size_ > 0 && consumed_size > 0) {
    // Case D. We consumed some frames but there is a leftover at the end of
    // the buffer. Shift out the consumed bytes, so that on the next round
    // |buf_| starts with the header of the next unconsumed frame.
    PERFETTO_DCHECK(rd_ptr > begin && rd_ptr + size_ <= end);
    memmove(buf_.get(), rd_ptr, size_);
  }
  // At this point |size_| == 0 for case C, > 0 for cases A, B, D.

  // There are two things we want to guarantee here:
  // 1. If a partial frame is received (either case A or B) we want to leave
  //    a decent capacity (kMinRecvBuffer) to the next recv(), to avoid
  //    fragmenting recv() calls.
  // 2. If the upcoming frame is larger than the current capacity, bump it
  //    in one go. At this point we know that we'll need exactly this much.
  // It could be also nice to shrink down the buffer after a large message has
  // been received. However, given that at current state that is unlikely to
  // happen, that would be useless extra complexity.
  size_t next_capacity = std::max(next_frame_size, size_ + kMinRecvBuffer);
  if (next_capacity > capacity_)
    SetCapacity(next_capacity);

  return true;
}

void BufferedFrameDeserializer::SetCapacity(size_t capacity) {
  PERFETTO_CHECK(capacity >= size_);
  char* old_buf = buf_.release();
  char* new_buf = reinterpret_cast<char*>(realloc(old_buf, capacity));
  PERFETTO_CHECK(new_buf);
  buf_.reset(new_buf);
  capacity_ = capacity;
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
