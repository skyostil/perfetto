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

// TODO(primiano): protobuf leaks CHECK macro, sigh.
#include "wire_protocol.pb.h"

#include <inttypes.h>

#include <utility>

#include "cpp_common/base.h"
#include "protorpc/src/rpc_frame_decoder.h"

namespace perfetto {
namespace protorpc {

RPCFrameDecoder::RPCFrameDecoder() = default;

std::pair<char*, size_t> RPCFrameDecoder::GetRecvBuffer() {
  // If this dcheck is hit the client has invoked two GetRecvBuffer() back to
  // back, without having called SetLastReadSize() in between.
  DCHECK(valid_size_ == buf_.size());
  const size_t kReadSize = 4096;
  buf_.resize(valid_size_ + kReadSize);
  return std::make_pair(buf_.data() + valid_size_, kReadSize);
}

void RPCFrameDecoder::SetLastReadSize(ssize_t rsize) {
  if (rsize <= 0) {
    buf_.resize(valid_size_);
    return;
  }
  buf_.resize(valid_size_ + rsize);
}

std::unique_ptr<RPCFrame> RPCFrameDecoder::GetRPCFrame() {
  // The header is just the number of bytes of the payload.
  const size_t kHeaderSize = sizeof(uint32_t);
  CHECK(valid_size_ <= buf_.size());
  if (valid_size_ < kHeaderSize)
    return nullptr;
  uint32_t frame_size = 0;
  memcpy(&frame_size, buf_.data(), kHeaderSize);
  frame_size = BYTE_SWAP_TO_LE32(frame_size);

  while (valid_size_ >= frame_size + kHeaderSize) {
    std::unique_ptr<RPCFrame> frame(new RPCFrame());
    bool decoded = frame->ParseFromArray(buf_.data() + kHeaderSize,
                                         static_cast<int>(frame_size));
    buf_.erase(buf_.begin(), buf_.begin() + kHeaderSize + frame_size);
    if (!decoded) {
      DLOG("Received malformed frame. size: %" PRIu32, frame_size);
      continue;
    }
    return frame;
  }
  return nullptr;  // Not enough data to decode the frame yet.
}

}  // namespace protorpc
}  // namespace perfetto
