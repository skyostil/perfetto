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

#ifndef PROTORPC_SRC_RPC_FRAME_DECODER_H_
#define PROTORPC_SRC_RPC_FRAME_DECODER_H_

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

namespace perfetto {
namespace protorpc {

class RPCFrame;

// Used by both and client to perform basic queueing and de-framing of the
// incoming socket data.
// Both ends of the RPC use this as follows:
//
// auto buf = rpc_frame_decoder.GetRecvBuffer();
// ssize_t rsize = socket.recv(buf.first, buf.second);
// rpc_frame_decoder.SetLastReadSize(rsize);
// while (rpc_frame_decoder.GetRPCFrame() != nullptr) {
//   ...
// }

// TODO(primiano): the current implementation is terribly inefficient as keeps
// reallocating all the time to expand and shrink the buffer. This can be made
// way more efficient by using a forward_list of fixed-side chunks and then
// using protobuf's io::ZeroCopyInputStream on top of it.
class RPCFrameDecoder {
 public:
  RPCFrameDecoder();

  // Return an empty buffer that can be passed to recv().
  std::pair<char*, size_t> GetRecvBuffer();
  void SetLastReadSize(ssize_t);
  std::unique_ptr<RPCFrame> GetRPCFrame();

 private:
  std::vector<char> buf_;

  // number of bytes (<= buf_.size()) that contain valid RPC frame data.
  // In stationary conditions valid_size_ == buf_.size(). Between a
  // GetRecvBuffer() and SetLastReadSize() call, it will become >= buf_.size().
  // In this case the remaining buf_.size() - valid_size_ are uninitialized
  // bytes that are handed to the client for the recv().
  size_t valid_size_ = 0;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_SRC_RPC_FRAME_DECODER_H_
