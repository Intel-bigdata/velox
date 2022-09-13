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

#include "velox/functions/prestosql/registration/RegistrationFunctions.h"
#include "velox/substrait/SubstraitSignature.h"

using namespace facebook::velox;
using namespace facebook::velox::substrait;

class SubstraitFunctionMappingsTest : public SubstraitFunctionMappings {
 public:
  const FunctionMappings scalarMappings() const override {
    static const FunctionMappings scalarMappings{
        {"plus", "add"},
    };
    return scalarMappings;
  }
};

class SubstraitSignatureTest : public ::testing::Test {
 protected:
  void assertSignature(
      const std::string& inputSignature,
      const std::string& outputSignature) {
    auto testSubstraitFunctionMappings =
        std::make_shared<SubstraitFunctionMappingsTest>();
    auto signature = SubstraitFunctionSignature ::signature(
        inputSignature, testSubstraitFunctionMappings);
    ASSERT_EQ(signature, outputSignature);
  }
};

TEST_F(SubstraitSignatureTest, signatureTest) {
  assertSignature("plus:opt_i8_i8", "add:opt_i8_i8");
  assertSignature("add:opt_i8_i8", "add:opt_i8_i8");
  assertSignature("add", "add");
}
