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

#ifndef IPC_SRC_BUFFERED_FRAME_DESERIALIZER_H_
#define IPC_SRC_BUFFERED_FRAME_DESERIALIZER_H_

#include <stddef.h>

#include <list>
#include <memory>
#include <utility>

#include "base/utils.h"

namespace perfetto {
namespace ipc {

class Frame;

// Deserializes incoming frames, taking care of buffering and tokenization.
// Used by both and client to decoded frames received on the socket.
//
// Which problem does it solve?
// ----------------------------
// The wire protocol is as follows:
// [32-bit frame size][proto-encoded Frame], e.g:
// [00 00 00 06][00 11 22 33 44 55 66]
// [00 00 00 02][AA BB]
// [00 00 00 04][CC DD EE FF]
// However, given that the socket works in SOCK_STREAM mode, the recv() calls
// might see the following:
// 00 00 00
// 06 00 11 22 33 44 55
// 66 00 00 00 02 ...
// This class takes care of buffering efficiently the data received, without
// making any assumption on how the incoming data will be chunked by the socket.
// For instance, it is possible that a recv doesn't produce any frame (because
// it received only a part of the frame) or produces >1 frame.
//
// Usage
// -----
// Both host and client use this as follows:
//
// auto buf = rpc_frame_decoder.BeginRecv();
// ssize_t rsize = socket.recv(buf.first, buf.second);
// rpc_frame_decoder.EndRecv(rsize);
// while (Frame frame = rpc_frame_decoder.GetRPCFrame()) {
//   ... process |frame|
// }
//
// Design goals:
// ---------------
// TODO. Explain that guarantees that every frame is in a virtually contiguous
// region, to avoid having to use protobuf full.
// TODO DOS prevention.
// No malloc for the common case.

class BufferedFrameDeserializer {
 public:
  BufferedFrameDeserializer();

  // Return an empty buffer that can be passed to recv().
  std::pair<char*, size_t> BeginRecv();

  // Must be called soon after BeginRecv() with the return value of recv().
  // If a header > kMaxFrameSize is received returns false. The caller is
  // expected to shutdown the socket and stop using this class at this point.
  bool EndRecv(size_t recv_size) __attribute__((warn_unused_result));

  // Decodes and returns the next decoded frame in the buffer if any, nullptr
  // if no further frames have been decoded.
  std::unique_ptr<Frame> PopNextFrame();

  size_t capacity() const { return capacity_; }
  size_t size() const { return size_; }

 private:
  void DecodeFrame(const char*, size_t);
  void SetCapacity(size_t);

  static constexpr size_t kMinRecvBuffer = 512;
  static constexpr size_t kMaxFrameSize = 16384;

  size_t capacity_ = 0;  // sizeof(|buf_|), buf_ can be partially uninitialized.
  size_t size_ = 0;  // <= capacity_. The number of EndRecv()'d bytes in |buf_|.
  std::unique_ptr<char, base::FreeDeleter> buf_;
  std::list<std::unique_ptr<Frame>> decoded_frames_;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // IPC_SRC_BUFFERED_FRAME_DESERIALIZER_H_
