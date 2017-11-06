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

#ifndef IPC_INCLUDE_IPC_DEFERRED_H_
#define IPC_INCLUDE_IPC_DEFERRED_H_

#include <assert.h>

#include <functional>
#include <memory>
#include <utility>

#include "ipc/basic_types.h"

namespace perfetto {
namespace ipc {

// This class is a wrapper for: (i) a callback, (ii) a protobuf message,
// (iii) an EOF-like boolean that tells whether more callbacks will follow (only
// for streaming responses, see https://grpc.io/docs/guides/concepts.html).
// The problem this is solving in the very essence is this. For any reply object
// of the methods generated from the .proto file:
//  - The client wants to see something on which it can bind a callback, invoked
//    when the reply to the method is received (or nack-ed in case of failure).
// - The host wants to see something on which it can set the result proto,
//   eventually more than once for streaming replies, and ship the result back.
//
// In both cases we want to make sure that callbacks don't get lost along the
// way. To address this, this class will automatically nack the callback (i.e.
// invoke it but pass a nullptr result object), unless .Resolve() is invoked or
// the object is std::move()'d.
// Imagine this as a class that can be either a Promise<T> or a Future<T>.
// It doesn't makes sense to maintain the complexity of both, as they would be
// quite overlapping.
//
// The client is supposed to use this class as follows:
// client.cc:
//   class GreeterProxy {
//      void SayHello(const HelloRequest&, Deferred<HelloReply> reply)
//   }
//  ...
//  Deferred<HelloReply> reply;
//  reply.Bind([] (Deferred<HelloReply> reply) {
//    std::cout << reply.success() ? reply->message : "failure";
//  });
//  host_proxy_instance.SayHello(req, std::move(reply));
//
// The host instead is supposed to use this as follows:
// host.cc
//   class GreeterImpl {
//     void SayHello(const HelloRequest& req, Deferred<HelloReply> reply) {
//        reply.msg()->set_greeting("Hello " + req.name)
//        reply.Resolve();
//      ...
//   }
// Or for more complex cases, the deferred object can be std::move()'d outside
// and the reply can continue asynchrnously later.

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

  virtual ~Deferred() { Fail(); }

  // Operators for std::move().

  // Can't just use =default here because the default move operator for
  // std::function doesn't necessarily swap and hence can leave a copy of the
  // bind state around.
  Deferred(Deferred&& other) noexcept { swap(other); }

  Deferred& operator=(Deferred&& other) {
    auto unresolved_callback = std::move(callback_);
    swap(other);
    if (unresolved_callback)
      unresolved_callback(Deferred<T>());  // Fail().
    return *this;
  }

  void swap(Deferred& other) {
    msg_ = std::move(other.msg_);
    has_more_ = other.has_more_;
    callback_ = std::move(other.callback_);

    other.callback_ = nullptr;
    other.has_more_ = false;
  }

  void Bind(std::function<void(Deferred<T>)> callback) {
    callback_ = std::move(callback);
  }

  // Invokes |callback_| passing the current |msg_| and |has_more_|. If no more
  // messages are expected, |callback_| is released.
  void Resolve() {
    if (!callback_)
      return;
    auto callback = std::move(callback_);
    callback_ = nullptr;
    callback(Deferred<T>(std::move(msg_), has_more_));
    if (has_more_)
      callback_ = std::move(callback);
  }

  // Resolves with a nullptr |msg_|, signalling failure to the other end.
  void Fail() {
    msg_.reset();
    has_more_ = false;
    Resolve();
  }

  bool success() const { return !!msg_; }
  bool is_bound() const { return !!callback_; }
  bool has_more() const { return has_more_; }
  void set_has_more(bool has_more) { has_more_ = has_more; }
  void set_msg(std::unique_ptr<ProtoMessage> r) { msg_ = std::move(r); }

  // Exposed publicly only for testing.
  T* unchecked_msg() { return static_cast<T*>(msg_.get()); }

  T* operator->() {
    assert(msg_);
    return unchecked_msg();
  }
  T& operator*() { return *(operator->()); }

  // Used to convert Deferred<SpecializedMessage> <-> Deferred<ProtoMessage>.
  // This is to allow the host/client logic to reason just in terms of
  // Deferred<ProtoMessage> and expose a Deferred<Specialized> to library
  // clients, so that they don't have to deal with downcasting themselves.
  // Note that downcasting is safe only if converting back and forth to the
  // same SpecializedMessage. This is meant to be called only by the
  // autogenerated code, hence the "Internal".
  template <typename X>
  Deferred<X> CovnvertInternal() {
    static_assert(std::is_base_of<ProtoMessage, X>::value, "X->ProtoMessage");
    auto orig_callback = std::move(callback_);
    callback_ = nullptr;
    auto callback_adapter = [orig_callback](Deferred<X> arg) {
      if (orig_callback)
        orig_callback(Deferred<T>(std::move(arg.msg_), arg.has_more_));
    };
    return Deferred<X>(std::move(msg_), has_more_, callback_adapter);
  }

 private:
  template <typename X>
  friend class Deferred;

  std::unique_ptr<ProtoMessage> msg_;
  bool has_more_ = false;
  std::function<void(Deferred<T>)> callback_;
};

}  // namespace ipc
}  // namespace perfetto

#endif  // IPC_INCLUDE_IPC_DEFERRED_H_
