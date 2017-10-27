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

#include "cpp_common/test/test_task_runner.h"
#include "protorpc/host.h"
#include "protorpc/src/test/greeter_service.h"

#include "gtest/gtest.h"

namespace perfetto {
namespace protorpc {
namespace {

const char kSocketName[] = "/tmp/test_protorpc";

class ProtoRPCTest : public ::testing::Test {
 protected:
  void SetUp() override { unlink(kSocketName); }
  void TearDown() override { unlink(kSocketName); }
};

TEST_F(ProtoRPCTest, GreeterService) {
  TestTaskRunner task_runner;
  std::shared_ptr<protorpc_test::GreeterServiceImpl> svc(new protorpc_test::GreeterServiceImpl());
  std::shared_ptr<Host> host(Host::CreateInstance(kSocketName, &task_runner));
  ASSERT_TRUE(host->ExposeService(svc));
  host->Start();
  task_runner.RunUntilIdle();
}

}  // namespace
}  // namespace protorpc
}  // namespace perfetto
