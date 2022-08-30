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
    extension_ = SubstraitExtension::loadExtension();
    mappings_ = std::make_shared<const VeloxToSubstraitFunctionMappings>();
    scalarFunctionLookup_ =
        std::make_shared<SubstraitScalarFunctionLookup>(extension_, mappings_);
    aggregateFunctionLookup_ =
        std::make_shared<SubstraitAggregateFunctionLookup>(
            extension_, mappings_);
    const auto& testExtension = SubstraitExtension::loadExtension(
        {getDataPath() + "functions_test.yaml"});
    testScalarFunctionLookup_ = std::make_shared<SubstraitScalarFunctionLookup>(
        testExtension, mappings_);
  }

  void testScalarFunctionLookup(
      const std::string& name,
      const std::vector<SubstraitTypePtr>& arguments,
      const SubstraitTypePtr& returnType,
      const std::string& outputSignature) {
    const auto& functionSignature =
        SubstraitFunctionSignature::of(name, arguments, returnType);
    const auto& functionOption =
        scalarFunctionLookup_->lookupFunction(functionSignature);

    ASSERT_TRUE(functionOption.has_value());
    ASSERT_EQ(functionOption.value()->anchor().key, outputSignature);
  }

  void testAggregateFunctionLookup(
      const std::string& name,
      const std::vector<SubstraitTypePtr>& arguments,
      const SubstraitTypePtr& returnType,
      const std::string& outputSignature) {
    const auto& functionSignature =
        SubstraitFunctionSignature::of(name, arguments, returnType);
    const auto& functionOption =
        aggregateFunctionLookup_->lookupFunction(functionSignature);

    ASSERT_TRUE(functionOption.has_value());
    ASSERT_EQ(functionOption.value()->anchor().key, outputSignature);
  }

  void assertTestSignature(
      const std::string& name,
      const std::vector<SubstraitTypePtr>& arguments,
      const SubstraitTypePtr& returnType,
      const std::string& outputSignature) {
    const auto& functionSignature =
        SubstraitFunctionSignature::of(name, arguments, returnType);
    const auto& functionOption =
        testScalarFunctionLookup_->lookupFunction(functionSignature);

    ASSERT_TRUE(functionOption.has_value());
    ASSERT_EQ(functionOption.value()->anchor().key, outputSignature);
  }

 private:
  static std::string getDataPath() {
    const std::string absolute_path = __FILE__;
    auto const pos = absolute_path.find_last_of('/');
    return absolute_path.substr(0, pos) + "/data/";
  }

  SubstraitExtensionPtr extension_;
  SubstraitFunctionMappingsPtr mappings_;
  SubstraitScalarFunctionLookupPtr scalarFunctionLookup_;
  SubstraitAggregateFunctionLookupPtr aggregateFunctionLookup_;
  SubstraitScalarFunctionLookupPtr testScalarFunctionLookup_;
};

TEST_F(SubstraitFunctionLookupTest, lt) {
  testScalarFunctionLookup("lt", {kI8(), kI8()}, kBool(), "lt:i8_i8");

  testScalarFunctionLookup("lt", {kI16(), kI16()}, kBool(), "lt:i16_i16");

  testScalarFunctionLookup("lt", {kI32(), kI32()}, kBool(), "lt:i32_i32");

  testScalarFunctionLookup("lt", {kI64(), kI64()}, kBool(), "lt:i64_i64");

  testScalarFunctionLookup("lt", {kFp32(), kFp32()}, kBool(), "lt:fp32_fp32");

  testScalarFunctionLookup("lt", {kFp64(), kFp64()}, kBool(), "lt:fp64_fp64");
}

TEST_F(SubstraitFunctionLookupTest, between) {
  testScalarFunctionLookup(
      "between", {kI8(), kI8(), kI8()}, kBool(), "between:i8_i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, add) {
  testScalarFunctionLookup("add", {kI8(), kI8()}, kI8(), "add:opt_i8_i8");

  testScalarFunctionLookup("plus", {kI8(), kI8()}, kI8(), "add:opt_i8_i8");
}

TEST_F(SubstraitFunctionLookupTest, devide) {
  testScalarFunctionLookup(
      "divide",
      {
          kFp32(),
          kFp32(),
      },
      kFp32(),
      "divide:opt_opt_fp32_fp32");
}

TEST_F(SubstraitFunctionLookupTest, test) {
  assertTestSignature(
      "test",
      {
          kFp32(),
          kFp32(),
      },
      kBool(),
      "test:fp32_fp32");

  assertTestSignature(
      "test",
      {
          kI8(),
          kI16(),
      },
      kBool(),
      "test:i8_i16");

  assertTestSignature(
      "test",
      {
          kI8(),
          kI16(),
          kI16(),
          kI32(),
      },
      kBool(),
      "test:i8_i16_i16_i32");

  assertTestSignature(
      "test",
      {
          kBool(),
          kBool(),
          kI16(),
          kI16(),
          kI8(),
      },
      kBool(),
      "test:bool_bool_i16_i16_i8");

  assertTestSignature(
      "test",
      {
          kBool(),
          kI8(),
          kBool(),
          kI16(),
          kI32(),
      },
      kBool(),
      "test:bool_i8_bool_i16_i32");

  assertTestSignature(
      "test",
      {
          kBool(),
          kI8(),
          kBool(),
          kI16(),
          kI32(),
      },
      kBool(),
      "test:bool_i8_bool_i16_i32");
}

TEST_F(SubstraitFunctionLookupTest, avg) {
  testAggregateFunctionLookup(
      "avg",
      {SubstraitType::decode("struct<fp64,i64>")},
      kFp64(),
      "avg:opt_fp32");
}

TEST_F(SubstraitFunctionLookupTest, logical) {
  testScalarFunctionLookup("and", {kBool(), kBool()}, kBool(), "and:bool");
  testScalarFunctionLookup("or", {kBool(), kBool()}, kBool(), "or:bool");
  testScalarFunctionLookup("not", {kBool()}, kBool(), "not:bool");
  testScalarFunctionLookup("xor", {kBool(), kBool()}, kBool(), "xor:bool_bool");
}
