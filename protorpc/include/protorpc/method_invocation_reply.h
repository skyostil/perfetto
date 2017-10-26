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

#ifndef PROTORPC_INCLUDE_PROTORPC_METHOD_INVOCATION_REPLY_H_
#define PROTORPC_INCLUDE_PROTORPC_METHOD_INVOCATION_REPLY_H_

#include <memory>
#include <utility>

#include "protorpc/basic_types.h"

namespace perfetto {
namespace protorpc {
// Wraps method invocation replies.

template <typename T = ProtoMessage>
class MethodInvocationReply {
 public:
  MethodInvocationReply(std::unique_ptr<ProtoMessage> result, bool eof)
      : result_(std::move(result)), eof_(eof) {
    static_assert(std::is_base_of<ProtoMessage, T>::value,
                  "T must be a ProtoMessage");
  }
  virtual ~MethodInvocationReply() = default;
  MethodInvocationReply(MethodInvocationReply&&) noexcept = default;
  MethodInvocationReply& operator=(MethodInvocationReply&&) = default;

  bool success() const { return !!result_; }
  bool eof() const { return eof_; }
  ProtoMessage* result() const { return result_.get(); }

  T& operator*() { return static_cast<T&>(*result_); }
  T* operator->() { return static_cast<T*>(result_.get()); }

  template <typename O>
  MethodInvocationReply<O> As() {
    return MethodInvocationReply<O>(
        std::unique_ptr<O>(static_cast<O*>(result_.release())), eof);
  }

 private:
  std::unique_ptr<ProtoMessage> result_;
  bool eof_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_METHOD_INVOCATION_REPLY_H_
