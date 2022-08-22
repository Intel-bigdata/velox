/*
 * Copyright (c) Facebook, Inc. and its affiliates.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "velox/common/base/tests/GTestUtils.h"

#include "velox/substrait/SubstraitType.h"

using namespace facebook::velox;
using namespace facebook::velox::substrait;

class SubstraitTypeTest : public ::testing::Test {};

TEST_F(SubstraitTypeTest, fromBool) {
  auto boolType = SubstraitType::decode("i8");
  ASSERT_TRUE(boolType->isKind());
  auto unknown = SubstraitType::decode("unknown");
  ASSERT_TRUE(unknown->isUnknown());
  auto any = SubstraitType::decode("any1");
  ASSERT_TRUE(any->isWildcard());
}

TEST_F(SubstraitTypeTest, fromVelox) {
  auto boolType = SubstraitType::fromVelox(BOOLEAN());
  ASSERT_TRUE(boolType->isKind());
}