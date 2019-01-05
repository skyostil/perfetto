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

/*******************************************************************************
 * AUTOGENERATED - DO NOT EDIT
 *******************************************************************************
 * This file has been generated from the protobuf message
 * perfetto/config/android/android_log_config.proto
 * by
 * ../../tools/proto_to_cpp/proto_to_cpp.cc.
 * If you need to make changes here, change the .proto file and then run
 * ./tools/gen_tracing_cpp_headers_from_protos
 */

#include "perfetto/tracing/core/android_log_config.h"

#include "perfetto/common/android_log_constants.pb.h"
#include "perfetto/config/android/android_log_config.pb.h"

namespace perfetto {

AndroidLogConfig::AndroidLogConfig() = default;
AndroidLogConfig::~AndroidLogConfig() = default;
AndroidLogConfig::AndroidLogConfig(const AndroidLogConfig&) = default;
AndroidLogConfig& AndroidLogConfig::operator=(const AndroidLogConfig&) =
    default;
AndroidLogConfig::AndroidLogConfig(AndroidLogConfig&&) noexcept = default;
AndroidLogConfig& AndroidLogConfig::operator=(AndroidLogConfig&&) = default;

void AndroidLogConfig::FromProto(
    const perfetto::protos::AndroidLogConfig& proto) {
  log_ids_.clear();
  for (const auto& field : proto.log_ids()) {
    log_ids_.emplace_back();
    static_assert(sizeof(log_ids_.back()) == sizeof(proto.log_ids(0)),
                  "size mismatch");
    log_ids_.back() = static_cast<decltype(log_ids_)::value_type>(field);
  }

  static_assert(sizeof(poll_ms_) == sizeof(proto.poll_ms()), "size mismatch");
  poll_ms_ = static_cast<decltype(poll_ms_)>(proto.poll_ms());

  static_assert(sizeof(min_prio_) == sizeof(proto.min_prio()), "size mismatch");
  min_prio_ = static_cast<decltype(min_prio_)>(proto.min_prio());

  filter_tags_.clear();
  for (const auto& field : proto.filter_tags()) {
    filter_tags_.emplace_back();
    static_assert(sizeof(filter_tags_.back()) == sizeof(proto.filter_tags(0)),
                  "size mismatch");
    filter_tags_.back() =
        static_cast<decltype(filter_tags_)::value_type>(field);
  }
  unknown_fields_ = proto.unknown_fields();
}

void AndroidLogConfig::ToProto(
    perfetto::protos::AndroidLogConfig* proto) const {
  proto->Clear();

  for (const auto& it : log_ids_) {
    proto->add_log_ids(static_cast<decltype(proto->log_ids(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->log_ids(0)), "size mismatch");
  }

  static_assert(sizeof(poll_ms_) == sizeof(proto->poll_ms()), "size mismatch");
  proto->set_poll_ms(static_cast<decltype(proto->poll_ms())>(poll_ms_));

  static_assert(sizeof(min_prio_) == sizeof(proto->min_prio()),
                "size mismatch");
  proto->set_min_prio(static_cast<decltype(proto->min_prio())>(min_prio_));

  for (const auto& it : filter_tags_) {
    proto->add_filter_tags(static_cast<decltype(proto->filter_tags(0))>(it));
    static_assert(sizeof(it) == sizeof(proto->filter_tags(0)), "size mismatch");
  }
  *(proto->mutable_unknown_fields()) = unknown_fields_;
}

}  // namespace perfetto
