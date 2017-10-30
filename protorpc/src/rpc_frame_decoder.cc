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

// TODO(primiano): protobuf leaks PERFETTO_CHECK macro, sigh.
#include "wire_protocol.pb.h"

#include <inttypes.h>

#include <utility>

#include "base/logging.h"
#include "base/utils.h"
#include "protorpc/src/rpc_frame_decoder.h"

namespace perfetto {
namespace protorpc {

RPCFrameDecoder::RPCFrameDecoder() = default;

std::pair<char*, size_t> RPCFrameDecoder::GetRecvBuffer() {
  // If this dcheck is hit the client has invoked two GetRecvBuffer() back to
  // back, without having called SetLastReadSize() in between.
  // PERFETTO_DLOG("GetRecvBuffer: %zu %zu", valid_size_, buf_.size());
  PERFETTO_DCHECK(valid_size_ == buf_.size());
  const size_t kReadSize = 4096;
  buf_.resize(valid_size_ + kReadSize);
  return std::make_pair(buf_.data() + valid_size_, kReadSize);
}

void RPCFrameDecoder::SetLastReadSize(ssize_t rsize) {
  PERFETTO_CHECK(
      rsize < 1024 * 1024);  // We don't expect recv() buffers to be that big.
  // PERFETTO_DLOG("SetLastReadSize: %zu %zu, rsize: %ld", valid_size_,
  // buf_.size(), rsize);
  if (rsize <= 0) {
    buf_.resize(valid_size_);
    return;
  }
  valid_size_ += rsize;
  buf_.resize(valid_size_);
}

std::unique_ptr<RPCFrame> RPCFrameDecoder::GetRPCFrame() {
  // The header is just the number of bytes of the payload.
  const size_t kHeaderSize = sizeof(uint32_t);
  // PERFETTO_DLOG("GetRPCFrame: %zu %zu", valid_size_, buf_.size());

  PERFETTO_CHECK(valid_size_ <= buf_.size());  // Sanity check.

  // This loop is only to skip any invalid frame. We can't just return nullptr
  // that case because the caller will assume that there are no more frame,
  // which might not be the case.
  // In other words, if the buffer contains an invalid frame followed by a valid
  // frame, we want to skip the invalid one and directly return the valid one.
  // TODO(primiano): add a test to cover this case before landing this.
  for (;;) {
    if (valid_size_ < kHeaderSize)
      return nullptr;  // There isn't enough data not even for the header.
    uint32_t frame_size = 0;
    memcpy(ASSUME_LITTLE_ENDIAN(&frame_size), buf_.data(), kHeaderSize);
    const size_t frame_size_including_header = kHeaderSize + frame_size;
    if (valid_size_ < frame_size_including_header)
      return nullptr;  // The header is here but the payload isn't complete yet.
    std::unique_ptr<RPCFrame> frame(new RPCFrame());
    bool decoded = frame->ParseFromArray(buf_.data() + kHeaderSize,
                                         static_cast<int>(frame_size));
    buf_.erase(buf_.begin(), buf_.begin() + frame_size_including_header);
    valid_size_ -= frame_size_including_header;
    if (decoded)
      return frame;
    PERFETTO_DLOG("Received malformed frame. size: %" PRIu32, frame_size);
  }
}

}  // namespace protorpc
}  // namespace perfetto
