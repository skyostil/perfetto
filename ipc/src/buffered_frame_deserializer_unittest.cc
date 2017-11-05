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

#include <algorithm>
#include <string>

#include "base/logging.h"
#include "base/utils.h"
#include "gtest/gtest.h"

#include "wire_protocol.pb.h"  // protobuf generated header.

namespace perfetto {
namespace ipc {
namespace {

// Generates a meaningless but parsable Frame of exactly |payload_size| bytes.
// The returned frame contains the 4 bytes header, so for a |payload_size| of 3
// bytes, this returns 4 + 3 = 7 bytes for the encoded frame and so on.
std::vector<char> GetSimpleFrame(size_t payload_size) {
  Frame frame;
  // A bit of reverse math of the proto encoding: a Frame which has only the
  // |data_for_testing| fields, will require for each data_for_testing that is
  // up to 127 bytes:
  // - 1 byte to write the field preamble (field type and id).
  // - 1 byte to write the field size, if 0 < size <= 127.
  // - N bytes for the actual content (|padding| below).
  // So below we split the payload into chunks of <= 127 bytes, keeping into
  // account the extra 2 bytes for each chunk.
  std::vector<char> padding;
  char padding_char = '0';
  for (size_t size_left = payload_size; size_left > 0;) {
    PERFETTO_CHECK(size_left >= 2);  // We cannot produce frames < 2 bytes.
    size_t padding_size;
    if (size_left <= 127) {
      padding_size = size_left - 2;
      size_left = 0;
    } else {
      padding_size = 124;
      size_left -= padding_size + 2;
    }
    padding.resize(padding_size);
    for (size_t i = 0; i < padding_size; i++) {
      padding_char = padding_char == 'z' ? '0' : padding_char + 1;
      padding[i] = padding_char;
    }
    frame.add_data_for_testing(padding.data(), padding_size);
  }
  PERFETTO_CHECK(payload_size == frame.ByteSize());
  std::vector<char> encoded_frame;
  encoded_frame.resize(sizeof(uint32_t) + payload_size);
  char* enc_buf = encoded_frame.data();
  PERFETTO_CHECK(
      frame.SerializeToArray(enc_buf + sizeof(uint32_t), payload_size));
  memcpy(enc_buf, base::AssumeLittleEndian(&payload_size), sizeof(uint32_t));
  PERFETTO_CHECK(sizeof(uint32_t) + payload_size == encoded_frame.size());
  return encoded_frame;
}

void SimulateRecv(std::pair<char*, size_t> rbuf,
                  const std::vector<char>& encoded_frame,
                  size_t offset = 0) {
  ASSERT_GE(rbuf.second, encoded_frame.size() + offset);
  memcpy(rbuf.first + offset, encoded_frame.data(), encoded_frame.size());
}

void AssertFrameEq(std::vector<char> expected_frame_with_header,
                   const Frame& frame) {
  std::string reserialized_frame = frame.SerializeAsString();
  ASSERT_EQ(expected_frame_with_header.size() - sizeof(uint32_t),
            reserialized_frame.size());
  ASSERT_EQ(0, memcmp(reserialized_frame.data(),
                      expected_frame_with_header.data() + sizeof(uint32_t),
                      reserialized_frame.size()));
}

// Creates a realistic Frame, simulates Recv() fragmenting it in three chunks
// and tests that the decoded Frame matches the original one.
TEST(BufferedFrameDeserializerTest, FragmentedFrameIsCorrectlyDeserialized) {
  BufferedFrameDeserializer bfd;
  Frame frame;
  frame.set_request_id(42);
  auto* bind_reply = frame.mutable_msg_bind_service_reply();
  bind_reply->set_success(true);
  bind_reply->set_service_id(0x4242);
  auto* method = bind_reply->add_methods();
  method->set_id(0x424242);
  method->set_name("foo");
  std::vector<char> serialized_frame;
  uint32_t payload_size = frame.ByteSize();

  serialized_frame.resize(sizeof(uint32_t) + payload_size);
  ASSERT_TRUE(frame.SerializeToArray(serialized_frame.data() + sizeof(uint32_t),
                                     payload_size));
  memcpy(serialized_frame.data(), base::AssumeLittleEndian(&payload_size),
         sizeof(uint32_t));

  std::vector<char> frame_chunk1(serialized_frame.begin(),
                                 serialized_frame.begin() + 5);
  std::pair<char*, size_t> rbuf = bfd.BeginRecv();
  SimulateRecv(rbuf, frame_chunk1);
  ASSERT_TRUE(bfd.EndRecv(frame_chunk1.size()));

  std::vector<char> frame_chunk2(serialized_frame.begin() + 5,
                                 serialized_frame.begin() + 10);
  rbuf = bfd.BeginRecv();
  SimulateRecv(rbuf, frame_chunk2);
  ASSERT_TRUE(bfd.EndRecv(frame_chunk2.size()));

  std::vector<char> frame_chunk3(serialized_frame.begin() + 10,
                                 serialized_frame.end());
  rbuf = bfd.BeginRecv();
  SimulateRecv(rbuf, frame_chunk3);
  ASSERT_TRUE(bfd.EndRecv(frame_chunk3.size()));

  // Validate the received frame.
  std::unique_ptr<Frame> decoded_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_frame);
  AssertFrameEq(serialized_frame, *decoded_frame);
}

// Tests the simple case where each recv() just returns one whole header+frame.
TEST(BufferedFrameDeserializerTest, WholeMessages) {
  BufferedFrameDeserializer bfd;
  for (int i = 1; i <= 50; i++) {
    const size_t payload_size = i * 5;
    std::pair<char*, size_t> rbuf = bfd.BeginRecv();

    ASSERT_NE(nullptr, rbuf.first);
    std::vector<char> frame = GetSimpleFrame(payload_size);
    SimulateRecv(rbuf, frame);
    ASSERT_TRUE(bfd.EndRecv(frame.size()));

    // Excactly one frame should be decoded, with no leftover buffer.
    ASSERT_TRUE(bfd.PopNextFrame());
    ASSERT_FALSE(bfd.PopNextFrame());
    ASSERT_EQ(0u, bfd.size());
  }
}

// Tests the case of a EndRecv(0) while receiving a valid frame in chunks.
TEST(BufferedFrameDeserializerTest, ZeroSizedRecv) {
  BufferedFrameDeserializer bfd;
  std::vector<char> frame = GetSimpleFrame(100);
  std::vector<char> frame_chunk1(frame.begin(), frame.begin() + 50);
  std::vector<char> frame_chunk2(frame.begin() + 50, frame.end());

  std::pair<char*, size_t> rbuf = bfd.BeginRecv();
  SimulateRecv(rbuf, frame_chunk1);
  ASSERT_TRUE(bfd.EndRecv(frame_chunk1.size()));

  rbuf = bfd.BeginRecv();
  ASSERT_TRUE(bfd.EndRecv(0));

  rbuf = bfd.BeginRecv();
  SimulateRecv(rbuf, frame_chunk2);
  ASSERT_TRUE(bfd.EndRecv(frame_chunk2.size()));

  // Excactly one frame should be decoded, with no leftover buffer.
  std::unique_ptr<Frame> decoded_frame = bfd.PopNextFrame();
  ASSERT_TRUE(decoded_frame);
  AssertFrameEq(frame, *decoded_frame);
  ASSERT_FALSE(bfd.PopNextFrame());
  ASSERT_EQ(0u, bfd.size());
}

// Test the case where a single Recv() returns batches of > 1 whole frames.
// See case C in the comments for BufferedFrameDeserializer::EndRecv().
TEST(BufferedFrameDeserializerTest, MultipleFramesInOneRecv) {
  BufferedFrameDeserializer bfd;
  std::vector<std::vector<size_t>> frame_batch_sizes(
      {{5}, {7, 11, 13}, {17}, {19, 23}});

  for (std::vector<size_t>& batch : frame_batch_sizes) {
    std::pair<char*, size_t> rbuf = bfd.BeginRecv();
    size_t frame_offset_in_batch = 0;
    for (size_t frame_size : batch) {
      auto frame = GetSimpleFrame(frame_size);
      SimulateRecv(rbuf, frame, frame_offset_in_batch);
      frame_offset_in_batch += frame.size();
    }
    ASSERT_TRUE(bfd.EndRecv(frame_offset_in_batch));
    for (size_t i = 0; i < batch.size(); i++)
      ASSERT_TRUE(bfd.PopNextFrame());
    ASSERT_FALSE(bfd.PopNextFrame());
    ASSERT_EQ(0u, bfd.size());
  }
}

TEST(BufferedFrameDeserializerTest, RejectVeryLargeFrames) {
  BufferedFrameDeserializer bfd;
  std::pair<char*, size_t> rbuf = bfd.BeginRecv();
  const uint32_t kBigSize = 32 * 1000 * 1000;
  memcpy(rbuf.first, base::AssumeLittleEndian(&kBigSize), sizeof(uint32_t));
  memcpy(rbuf.first + sizeof(uint32_t), "some initial payload", 20);
  ASSERT_FALSE(bfd.EndRecv(sizeof(uint32_t) + 20));
}

// Tests the extreme case of recv() fragmentation. Two valid frames are received
// but each recv() puts one byte at a time. Covers cases A and B commented in
// BufferedFrameDeserializer::EndRecv().
TEST(BufferedFrameDeserializerTest, HighlyFragmentedFrames) {
  BufferedFrameDeserializer bfd;
  for (int i = 1; i <= 50; i++) {
    std::vector<char> frame = GetSimpleFrame(i * 100);
    for (size_t off = 0; off < frame.size(); off++) {
      std::pair<char*, size_t> rbuf = bfd.BeginRecv();
      SimulateRecv(rbuf, {frame[off]});

      // The frame should be available only when receiving the last byte.
      ASSERT_TRUE(bfd.EndRecv(1));
      if (off < frame.size() - 1) {
        ASSERT_FALSE(bfd.PopNextFrame()) << off << "/" << frame.size();
        ASSERT_EQ(off + 1, bfd.size());
      } else {
        ASSERT_TRUE(bfd.PopNextFrame());
      }
    }
  }
}

// A bunch of valid frames interleaved with frames that have a valid header
// but unparsable payload. The expectation is that PopNextFrame() returns
// nullptr for the unparsable frames but the other frames are decoded peroperly.
TEST(BufferedFrameDeserializerTest, CanRecoverAfterUnparsableFrames) {
  BufferedFrameDeserializer bfd;
  for (int i = 1; i <= 50; i++) {
    const size_t payload_size = i * 5;
    std::pair<char*, size_t> rbuf = bfd.BeginRecv();

    ASSERT_NE(nullptr, rbuf.first);
    std::vector<char> frame = GetSimpleFrame(payload_size);
    ASSERT_EQ(payload_size + sizeof(uint32_t), frame.size());
    ASSERT_GE(rbuf.second, frame.size());
    memcpy(rbuf.first, frame.data(), frame.size());  // fake the recv().
    const bool unparsable = (i % 3) == 1;
    if (unparsable)
      memset(rbuf.first + sizeof(uint32_t), 0xFF, payload_size);
    ASSERT_TRUE(bfd.EndRecv(frame.size()));

    // Excactly one frame should be decoded if |parsable|. In any case no
    // leftover bytes should be left in the buffer.
    ASSERT_EQ(!unparsable, !!bfd.PopNextFrame());
    ASSERT_EQ(0u, bfd.size());
  }
}

// Test the case of recv()s that always max up the max_capacity.
// Alternate the receival of one recv() that returns two frames which total
// size is exactly |max_capacity| and one recv() that returns one frame, also
// big |max_capacity|.
TEST(BufferedFrameDeserializerTest, FillCapacity) {
  size_t max_capacity = 1024 * 16;
  BufferedFrameDeserializer bfd(max_capacity);

  for (int i = 0; i < 3; i++) {
    std::pair<char*, size_t> rbuf = bfd.BeginRecv();
    std::vector<char> frame1 = GetSimpleFrame(1024);
    std::vector<char> frame2 =
        GetSimpleFrame(max_capacity - frame1.size() - sizeof(uint32_t));
    SimulateRecv(rbuf, frame1);
    SimulateRecv(rbuf, frame2, frame1.size());
    ASSERT_TRUE(bfd.EndRecv(frame1.size() + frame2.size()));

    rbuf = bfd.BeginRecv();
    std::vector<char> frame3 = GetSimpleFrame(max_capacity - sizeof(uint32_t));
    SimulateRecv(rbuf, frame3);
    ASSERT_TRUE(bfd.EndRecv(frame3.size()));

    std::unique_ptr<Frame> decoded_frame_1 = bfd.PopNextFrame();
    std::unique_ptr<Frame> decoded_frame_2 = bfd.PopNextFrame();
    std::unique_ptr<Frame> decoded_frame_3 = bfd.PopNextFrame();
    ASSERT_TRUE(decoded_frame_1);
    AssertFrameEq(frame1, *decoded_frame_1);

    ASSERT_TRUE(decoded_frame_2);
    AssertFrameEq(frame2, *decoded_frame_2);

    ASSERT_TRUE(decoded_frame_3);
    AssertFrameEq(frame3, *decoded_frame_3);
  }
}

}  // namespace
}  // namespace ipc
}  // namespace perfetto
