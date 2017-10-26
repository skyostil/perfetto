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

#ifndef PROTORPC_SRC_TEST_GREETER_SERVICE_H_
#define PROTORPC_SRC_TEST_GREETER_SERVICE_H_

#include "greeter_service.pb.h"

#include "protorpc/service.h"
#include "protorpc/src/test/greeter_service-gen.h"

// Deliberately using a namespace != protorpc to spot subtle namespace dep bugs.
namespace protorpc_test {

class GreeterServiceImpl : public Greeter::Service {
 public:
  ~GreeterServiceImpl() override;
  void SayHello(GreeterRequest, GreeterReply) override;
  void WaveGoodBye(GreeterRequest, GreeterReply) override;
};

}  // namespace protorpc_test

#endif  // PROTORPC_SRC_TEST_GREETER_SERVICE_H_
