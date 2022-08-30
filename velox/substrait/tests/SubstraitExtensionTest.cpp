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

#include "velox/substrait/SubstraitExtension.h"

using namespace facebook::velox;
using namespace facebook::velox::substrait;

class SubstraitExtensionTest : public ::testing::Test {};

/// used to lookup function with user provided function mappings.
class SubstraitFunctionMappingsTest : public SubstraitFunctionMappings {
 public:
  const FunctionMappings scalarMappings() const override {
    static const FunctionMappings scalarMappings{
        {"plus", "add"},
    };
    return scalarMappings;
  }

  const FunctionMappings aggregateMappings() const override {
    return facebook::velox::substrait::FunctionMappings();
  }
  const FunctionMappings windowMappings() const override {
    return facebook::velox::substrait::FunctionMappings();
  }
};

TEST_F(SubstraitExtensionTest, loadExtension) {
  auto extension = SubstraitExtension::loadExtension();
  /// currently we have 198 scalar function variants defined in substrait
  /// extension
  ASSERT_EQ(extension->scalarFunctionVariants.size(), 198);
  /// currently we have 46 aggregate function variants defined in substrait
  /// extension
  ASSERT_EQ(extension->aggregateFunctionVariants.size(), 46);
}

TEST_F(SubstraitExtensionTest, lookupFunction) {
  auto extension = SubstraitExtension::loadExtension();
  const auto& function = extension->lookupFunction("add:opt_i8_i8");
  ASSERT_TRUE(function.has_value());
  ASSERT_EQ(function.value()->signature(), "add:opt_i8_i8");
}

TEST_F(SubstraitExtensionTest, lookupFunctionWithMappings) {
  auto extension = SubstraitExtension::loadExtension();
  auto testSubstraitFunctionMappings =
      std::make_shared<SubstraitFunctionMappingsTest>();
  const auto& function = extension->lookupFunction(
      testSubstraitFunctionMappings, "plus:opt_i8_i8");
  ASSERT_TRUE(function.has_value());
  ASSERT_EQ(function.value()->signature(), "add:opt_i8_i8");
}
