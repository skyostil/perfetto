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

#include "ftrace_paths.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

using testing::HasSubstr;
using testing::Not;

namespace perfetto {
namespace {

TEST(FtracePaths, Paths) {
  const FtracePaths paths("/tracing/");
  EXPECT_EQ("/tracing/trace_marker", paths.trace_marker());
  EXPECT_EQ("/tracing/trace", paths.trace());
  EXPECT_EQ("/tracing/tracing_on", paths.tracing_on());
  EXPECT_EQ("/tracing/available_events", paths.available_events());
  EXPECT_EQ("/tracing/events/FOO/BAR/enable", paths.Enable("FOO", "BAR"));
  EXPECT_EQ("/tracing/events/FOO/BAR/format", paths.Format("FOO", "BAR"));
  EXPECT_EQ("/tracing/per_cpu/0/trace_pipe_raw", paths.TracePipeRaw(0));
  EXPECT_EQ("/tracing/per_cpu/123/trace_pipe_raw", paths.TracePipeRaw(123));
}

TEST(FtracePaths, HandlesDifferentRoots) {
  const FtracePaths paths("/foo/bar/tracing/");
  EXPECT_EQ("/foo/bar/tracing/trace_marker", paths.trace_marker());
}

}  // namespace
}  // namespace perfetto
