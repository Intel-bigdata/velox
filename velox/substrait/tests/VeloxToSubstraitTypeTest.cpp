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

#include "velox/substrait/SubstraitParser.h"
#include "velox/substrait/TypeUtils.h"
#include "velox/substrait/VeloxToSubstraitType.h"

using namespace facebook::velox;
using namespace facebook::velox::substrait;

namespace facebook::velox::substrait::test {

class VeloxToSubstraitTypeTest : public ::testing::Test {
 protected:
  void testTypeConversion(const TypePtr& type) {
    SCOPED_TRACE(type->toString());

    google::protobuf::Arena arena;
    auto substraitType = typeConvertor_->toSubstraitType(arena, type);
    auto sameType =
        toVeloxType(substraitParser_->parseType(substraitType)->type);
    ASSERT_TRUE(sameType->kindEquals(type))
        << "Expected: " << type->toString()
        << ", but got: " << sameType->toString();
  }

  template <SubstraitTypeKind kind>
  void testFromVelox(const TypePtr& type) {
    const auto& substraitType = fromVelox(type);
    ASSERT_EQ(substraitType->kind(), kind);
  }

  template <class T>
  void testFromVelox(
      const TypePtr& type,
      const std::function<void(const std::shared_ptr<const T>&)>&
          typeCallBack) {
    const auto& substraitType = fromVelox(type);
    if (typeCallBack) {
      typeCallBack(std::dynamic_pointer_cast<const T>(substraitType));
    }
  }

  std::shared_ptr<VeloxToSubstraitTypeConvertor> typeConvertor_;

  std::shared_ptr<SubstraitParser> substraitParser_ =
      std::make_shared<SubstraitParser>();
};

TEST_F(VeloxToSubstraitTypeTest, basic) {
  testTypeConversion(BOOLEAN());

  testTypeConversion(TINYINT());
  testTypeConversion(SMALLINT());
  testTypeConversion(INTEGER());
  testTypeConversion(BIGINT());

  testTypeConversion(REAL());
  testTypeConversion(DOUBLE());

  testTypeConversion(VARCHAR());
  testTypeConversion(VARBINARY());

  testTypeConversion(ARRAY(BIGINT()));
  testTypeConversion(MAP(BIGINT(), DOUBLE()));

  testTypeConversion(ROW({"a", "b", "c"}, {BIGINT(), BOOLEAN(), VARCHAR()}));
  testTypeConversion(
      ROW({"a", "b", "c"},
          {BIGINT(), ROW({"x", "y"}, {BOOLEAN(), VARCHAR()}), REAL()}));
  ASSERT_ANY_THROW(testTypeConversion(ROW({}, {})));
}

TEST_F(VeloxToSubstraitTypeTest, fromVeloxTest) {
  testFromVelox<SubstraitTypeKind::kBool>(BOOLEAN());
  testFromVelox<SubstraitTypeKind::kI8>(TINYINT());
  testFromVelox<SubstraitTypeKind::kI16>(SMALLINT());
  testFromVelox<SubstraitTypeKind::kI32>(INTEGER());
  testFromVelox<SubstraitTypeKind::kI64>(BIGINT());
  testFromVelox<SubstraitTypeKind::kFp32>(REAL());
  testFromVelox<SubstraitTypeKind::kFp64>(DOUBLE());
  testFromVelox<SubstraitTypeKind::kTimestamp>(TIMESTAMP());
  testFromVelox<SubstraitTypeKind::kDate>(DATE());

  testFromVelox<SubstraitTypeKind::kIntervalDay>(INTERVAL_DAY_TIME());

  testFromVelox<SubstraitStructType>(
      ROW({"a", "b"}, {TINYINT(), INTEGER()}),
      [](const std::shared_ptr<const SubstraitStructType>& typePtr) {
        ASSERT_TRUE(typePtr->children().size() == 2);
        ASSERT_TRUE(typePtr->children()[0]->kind() == SubstraitTypeKind::kI8);
        ASSERT_TRUE(typePtr->children()[1]->kind() == SubstraitTypeKind::kI32);
      });

  testFromVelox<SubstraitStructType>(
      ROW({"a", "b"}, {TINYINT(), ROW({INTEGER(), BIGINT()})}),
      [](const std::shared_ptr<const SubstraitStructType>& typePtr) {
        ASSERT_TRUE(typePtr->children().size() == 2);
        ASSERT_TRUE(typePtr->children()[0]->kind() == SubstraitTypeKind::kI8);
        ASSERT_TRUE(
            typePtr->children()[1]->kind() == SubstraitTypeKind::kStruct);
      });

  testFromVelox<SubstraitListType>(
      ARRAY({TINYINT()}),
      [](const std::shared_ptr<const SubstraitListType>& typePtr) {
        ASSERT_TRUE(typePtr->type()->kind() == SubstraitTypeKind ::kI8);
      });

  testFromVelox<SubstraitMapType>(
      MAP(INTEGER(), BIGINT()),
      [](const std::shared_ptr<const SubstraitMapType>& typePtr) {
        ASSERT_TRUE(typePtr->keyType()->kind() == SubstraitTypeKind ::kI32);
        ASSERT_TRUE(typePtr->valueType()->kind() == SubstraitTypeKind ::kI64);
      });
}

} // namespace facebook::velox::substrait::test
