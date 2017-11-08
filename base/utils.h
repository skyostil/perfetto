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

#ifndef PERFETTO_BASE_UTILS_H_
#define PERFETTO_BASE_UTILS_H_

#include <errno.h>
#include <stddef.h>
#include <stdlib.h>

#include <memory>
#include <utility>

#define PERFETTO_EINTR(x)                                   \
  ({                                                        \
    decltype(x) eintr_wrapper_result;                       \
    do {                                                    \
      eintr_wrapper_result = (x);                           \
    } while (eintr_wrapper_result == -1 && errno == EINTR); \
    eintr_wrapper_result;                                   \
  })

namespace perfetto {
namespace base {

template <typename T>
constexpr size_t ArraySize(const T& array) {
  return sizeof(array) / sizeof(array[0]);
}

template <typename... T>
inline void ignore_result(const T&...) {}

// Function object which invokes 'free' on its parameter, which must be
// a pointer. Can be used to store malloc-allocated pointers in std::unique_ptr:
//
// std::unique_ptr<int, base::FreeDeleter> foo_ptr(
//     static_cast<int*>(malloc(sizeof(int))));
struct FreeDeleter {
  inline void operator()(void* ptr) const {
    free(ptr);
  }
};

namespace {

template <typename T>
struct MakeUniqueResult {
  using Scalar = std::unique_ptr<T>;
};

template <typename T>
struct MakeUniqueResult<T[]> {
  using Array = std::unique_ptr<T[]>;
};

template <typename T, size_t N>
struct MakeUniqueResult<T[N]> {
  using Invalid = void;
};

}  // namespace

// Overload for non-array types. Arguments are forwarded to T's constructor.
template <typename T, typename... Args>
typename MakeUniqueResult<T>::Scalar MakeUnique(Args&&... args) {
  return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

// Overload for array types of unknown bound, e.g. T[]. The array is allocated
// with `new T[n]()` and value-initialized: note that this is distinct from
// `new T[n]`, which default-initializes.
template <typename T>
typename MakeUniqueResult<T>::Array MakeUnique(size_t size) {
  return std::unique_ptr<T>(new typename std::remove_extent<T>::type[size]());
}

// Overload to reject array types of known bound, e.g. T[n].
template <typename T, typename... Args>
typename MakeUniqueResult<T>::Invalid MakeUnique(Args&&... args) = delete;

}  // namespace base
}  // namespace perfetto

#endif  // PERFETTO_BASE_UTILS_H_
