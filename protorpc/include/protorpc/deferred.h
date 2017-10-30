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

#ifndef PROTORPC_INCLUDE_PROTORPC_DEFERRED_H_
#define PROTORPC_INCLUDE_PROTORPC_DEFERRED_H_

#include <assert.h>

#include <functional>
#include <memory>
#include <utility>

#include "protorpc/basic_types.h"

namespace perfetto {
namespace protorpc {

template <typename T = ProtoMessage>
class Deferred {
 public:
  Deferred(std::unique_ptr<ProtoMessage> msg = nullptr,
           bool has_more = false,
           std::function<void(Deferred<T>)> callback = nullptr)
      : msg_(std::move(msg)),
        has_more_(has_more),
        callback_(std::move(callback)) {
    static_assert(std::is_base_of<ProtoMessage, T>::value, "T->ProtoMessage");
  }

  virtual ~Deferred() {
    if (!callback_)
      return;
    auto callback = std::move(callback_);
    callback_ = nullptr;
    callback(Deferred<T>());
  }

  Deferred(Deferred&& other) noexcept {
    msg_ = std::move(other.msg_);
    has_more_ = other.has_more_;
    callback_ = std::move(other.callback_);

    // std::move(std::function) won't necessarily clear up the bind state.
    other.callback_ = nullptr;
    other.has_more_ = false;
  }

  Deferred& operator=(Deferred&& other) {
    auto prev_callback = std::move(callback_);
    msg_ = std::move(other.msg_);
    has_more_ = other.has_more_;
    callback_ = std::move(other.callback_);
    other.callback_ = nullptr;
    other.has_more_ = false;
    if (prev_callback)
      prev_callback(Deferred<T>());
  }

  bool success() const { return !!msg_; }
  bool has_more() const { return has_more_; }
  void set_has_more(bool has_more) { has_more_ = has_more; }
  void set_msg(std::unique_ptr<ProtoMessage> r) { msg_ = std::move(r); }

  std::unique_ptr<ProtoMessage>* msg() { return &msg_; }
  T* operator->() {
    assert(msg_);
    return static_cast<T*>(msg_.get());
  }
  T& operator*() { return *this; }

  void Bind(std::function<void(Deferred<T>)> callback) {
    callback_ = std::move(callback);
  }

  void Resolve() {
    if (!callback_)
      return;
    auto callback = std::move(callback_);
    callback_ = nullptr;
    callback(Deferred<T>(std::move(msg_), has_more_));
    if (has_more_)
      callback_ = std::move(callback);
  }

  template <typename X>
  Deferred<X> As() {
    static_assert(std::is_base_of<ProtoMessage, X>::value, "X->ProtoMessage");
    auto orig_callback = std::move(callback_);
    callback_ = nullptr;
    auto callback_adapter = [orig_callback](Deferred<X> arg) {
      orig_callback(Deferred<T>(std::move(*arg.msg()), arg.has_more()));
    };
    return Deferred<X>(std::move(msg_), has_more_, callback_adapter);
  }

 private:
  std::unique_ptr<ProtoMessage> msg_;
  bool has_more_ = false;
  std::function<void(Deferred<T>)> callback_;
};

}  // namespace protorpc
}  // namespace perfetto

#endif  // PROTORPC_INCLUDE_PROTORPC_DEFERRED_H_
