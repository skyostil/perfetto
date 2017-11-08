#!/bin/bash

# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

set -e

function build_config {
  local name=$1
  mkdir -p out/$name
  cat > out/$name/args.gn
  gn gen out/$name
  gn check out/$name
  ninja -C out/$name -j100
  out/$name/base_unittests
  out/$name/protozero_unittests
  out/$name/tracing_unittests
  out/$name/sanitizers_unittests
  out/$name/ftrace_reader_unittests
}

build_config prod <<EOF
is_debug=false
EOF

build_config debug <<EOF
is_debug=true
EOF

build_config asan <<EOF
is_debug=false
is_asan=true
EOF

build_config msan <<EOF
is_debug=false
is_msan=true
EOF

build_config lsan <<EOF
is_debug=false
is_lsan=true
EOF

build_config ubsan <<EOF
is_debug=false
is_ubsan=true
EOF

# gcc is broken currently.
# build_config gcc <<EOF
# is_debug=false
# is_clang=false
# EOF
