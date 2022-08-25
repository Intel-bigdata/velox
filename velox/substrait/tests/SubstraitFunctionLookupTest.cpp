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

#include "velox/substrait/SubstraitFunctionLookup.h"
#include "velox/substrait/VeloxToSubstraitMappings.h"

using namespace facebook::velox;
using namespace facebook::velox::substrait;

class SubstraitFunctionLookupTest : public ::testing::Test {
 protected:
  void SetUp() override {
    extension = SubstraitExtension::loadExtension();
    mappings = std::make_shared<const VeloxToSubstraitFunctionMappings>();
    scalarFunctionLookup =
        std::make_shared<SubstraitScalarFunctionLookup>(extension, mappings);
    aggregateFunctionLookup =
        std::make_shared<SubstraitAggregateFunctionLookup>(extension, mappings);
    const auto& testExtension = SubstraitExtension::loadExtension(
        {getDataPath() + "functions_test.yaml"});
    testScalarFunctionLookup = std::make_shared<SubstraitScalarFunctionLookup>(
        testExtension, mappings);
  }

 private:
  std::string getDataPath() {
    const std::string absolute_path = __FILE__;
    auto const pos = absolute_path.find_last_of('/');
    return absolute_path.substr(0, pos) + "/data/";
  }

 public:
  SubstraitExtensionPtr extension;
  SubstraitFunctionMappingsPtr mappings;
  SubstraitScalarFunctionLookupPtr scalarFunctionLookup;
  SubstraitAggregateFunctionLookupPtr aggregateFunctionLookup;
  SubstraitScalarFunctionLookupPtr testScalarFunctionLookup;
};

TEST_F(SubstraitFunctionLookupTest, lt_i8_i8) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",

      {SUBSTRAIT_TYPE_OF(kI8), SUBSTRAIT_TYPE_OF(kI8)});
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, lt_i16_i16) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",
      {SUBSTRAIT_TYPE_OF(kI16), SUBSTRAIT_TYPE_OF(kI16)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:i16_i16");
}

TEST_F(SubstraitFunctionLookupTest, lt_i32_i32) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",
      {SUBSTRAIT_TYPE_OF(kI32), SUBSTRAIT_TYPE_OF(kI32)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:i32_i32");
}

TEST_F(SubstraitFunctionLookupTest, lt_i64_i64) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",
      {SUBSTRAIT_TYPE_OF(kI64), SUBSTRAIT_TYPE_OF(kI64)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:i64_i64");
}

TEST_F(SubstraitFunctionLookupTest, lt_fp32_fp32) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",
      {SUBSTRAIT_TYPE_OF(kFp32), SUBSTRAIT_TYPE_OF(kFp32)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:fp32_fp32");
}

TEST_F(SubstraitFunctionLookupTest, between_i8_i8_i8) {
  const auto& signature = SubstraitFunctionSignature::of(
      "between",
      {SUBSTRAIT_TYPE_OF(kI8), SUBSTRAIT_TYPE_OF(kI8), SUBSTRAIT_TYPE_OF(kI8)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "between");
  ASSERT_EQ(functionOption.value()->anchor().key, "between:i8_i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, lt_fp64_fp64) {
  const auto& signature = SubstraitFunctionSignature::of(
      "lt",
      {SUBSTRAIT_TYPE_OF(kFp64), SUBSTRAIT_TYPE_OF(kFp64)},
      SUBSTRAIT_TYPE_OF(kBool));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "lt");
  ASSERT_EQ(functionOption.value()->anchor().key, "lt:fp64_fp64");
}

TEST_F(SubstraitFunctionLookupTest, add_i8_i8) {
  const auto& signature = SubstraitFunctionSignature::of(
      "add",
      {SUBSTRAIT_TYPE_OF(kI8), SUBSTRAIT_TYPE_OF(kI8)},
      SUBSTRAIT_TYPE_OF(kI8));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "add:opt_i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, plus_i8_i8) {
  const auto& signature = SubstraitFunctionSignature::of(
      "plus",
      {SUBSTRAIT_TYPE_OF(kI8), SUBSTRAIT_TYPE_OF(kI8)},
      SUBSTRAIT_TYPE_OF(kI8));
  auto functionOption = scalarFunctionLookup->lookupFunction(signature);
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "add:opt_i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, plus_i8_i8_i8) {
  auto functionOption =
      scalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "plus",
          {
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kI8),
          },

          SUBSTRAIT_TYPE_OF(kI8)));
  // it should match with I8 type
  ASSERT_FALSE(functionOption.has_value());
}

TEST_F(SubstraitFunctionLookupTest, add_i8) {
  auto functionOption =
      scalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "add",
          {
              SUBSTRAIT_TYPE_OF(kI8),
          },
          SUBSTRAIT_TYPE_OF(kI8)));
  // it should match with I8 type
  ASSERT_FALSE(functionOption.has_value());
}

TEST_F(SubstraitFunctionLookupTest, devide_fp32_fp32_with_rounding) {
  auto functionOption =
      scalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "divide",
          {
              SUBSTRAIT_TYPE_OF(kFp32),
              SUBSTRAIT_TYPE_OF(kFp32),
          },
          SUBSTRAIT_TYPE_OF(kFp32)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "divide:opt_opt_fp32_fp32");
}

TEST_F(SubstraitFunctionLookupTest, lookupWithAny1Any1) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kFp32),
              SUBSTRAIT_TYPE_OF(kFp32),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "test:fp32_fp32");
}

TEST_F(SubstraitFunctionLookupTest, lookupWithAny1Any2) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kI16),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "test:i8_i16");
}

TEST_F(SubstraitFunctionLookupTest, lookupWithi8_any1_any1_any2) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kI32),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "test:i8_i16_i16_i32");
}

TEST_F(SubstraitFunctionLookupTest, lookupWithany1_any1_any2_any2_i8) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kI8),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "test:bool_bool_i16_i16_i8");
}

TEST_F(SubstraitFunctionLookupTest, lookupWith_any1_i8_any1_i16_any2) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kI32),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_EQ(functionOption.value()->anchor().key, "test:bool_i8_bool_i16_i32");
}

TEST_F(SubstraitFunctionLookupTest, lookupFailWith_any1_i8_any1_i16_any2) {
  auto functionOption =
      testScalarFunctionLookup->lookupFunction(SubstraitFunctionSignature::of(
          "test",
          {
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kI8),
              SUBSTRAIT_TYPE_OF(kBool),
              SUBSTRAIT_TYPE_OF(kI16),
              SUBSTRAIT_TYPE_OF(kBool),
          },
          SUBSTRAIT_TYPE_OF(kBool)));
  // it should match with I8 type
  ASSERT_FALSE(functionOption.has_value());
}

TEST_F(SubstraitFunctionLookupTest, avg_struct_fp64_i64) {
  const auto& signature = SubstraitFunctionSignature::of(
      "avg",
      {SubstraitType::decode("struct<fp64,i64>")},
      SUBSTRAIT_TYPE_OF(kFp64));
  auto functionOption = aggregateFunctionLookup->lookupFunction(signature);

  // it should match with any type
  ASSERT_TRUE(functionOption.has_value());
  ASSERT_TRUE(functionOption.value()->name == "avg");
  ASSERT_EQ(functionOption.value()->anchor().key, "avg:opt_fp32");
}
