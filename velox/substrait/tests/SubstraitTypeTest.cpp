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

TEST_F(SubstraitTypeTest, bool_test) {
  auto boolType = SubstraitType::decode("boolean");
  ASSERT_TRUE(boolType->kind() == SubstraitTypeKind::kBool);
  ASSERT_EQ(boolType->signature(), "bool");

  boolType = SubstraitType::decode("BOOLEAN");
  ASSERT_TRUE(boolType->kind() == SubstraitTypeKind::kBool);
  ASSERT_EQ(boolType->signature(), "bool");
}

TEST_F(SubstraitTypeTest, i8_test) {
  auto type = SubstraitType::decode("i8");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kI8);
  ASSERT_EQ(type->signature(), "i8");
}

TEST_F(SubstraitTypeTest, i16_test) {
  auto type = SubstraitType::decode("i16");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kI16);
  ASSERT_EQ(type->signature(), "i16");
}

TEST_F(SubstraitTypeTest, i32_test) {
  auto type = SubstraitType::decode("i32");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kI32);
  ASSERT_EQ(type->signature(), "i32");
}

TEST_F(SubstraitTypeTest, i64_test) {
  auto type = SubstraitType::decode("i64");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kI64);
  ASSERT_EQ(type->signature(), "i64");
}

TEST_F(SubstraitTypeTest, fp32_test) {
  auto type = SubstraitType::decode("fp32");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kFp32);
  ASSERT_EQ(type->signature(), "fp32");
}

TEST_F(SubstraitTypeTest, fp64_test) {
  auto type = SubstraitType::decode("fp64");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kFp64);
  ASSERT_EQ(type->signature(), "fp64");
}

TEST_F(SubstraitTypeTest, decimal_test) {
  auto type = SubstraitType::decode("decimal<P1,S1>");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kDecimal);
  auto decimalType =
      std::dynamic_pointer_cast<const SubstraitDecimalType>(type);
  ASSERT_EQ(decimalType->signature(), "dec<P1,S1>");
  ASSERT_EQ(decimalType->precision(), "P1");
  ASSERT_EQ(decimalType->scale(), "S1");
}

TEST_F(SubstraitTypeTest, string_test) {
  auto type = SubstraitType::decode("string");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kString);
  ASSERT_EQ(type->signature(), "str");
}

TEST_F(SubstraitTypeTest, binary_test) {
  auto type = SubstraitType::decode("binary");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kBinary);
  ASSERT_EQ(type->signature(), "vbin");
}

TEST_F(SubstraitTypeTest, timestamp_test) {
  auto type = SubstraitType::decode("timestamp");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kTimestamp);
  ASSERT_EQ(type->signature(), "ts");
}

TEST_F(SubstraitTypeTest, timestamp_tz_test) {
  auto type = SubstraitType::decode("timestamp_tz");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kTimestampTz);
  ASSERT_EQ(type->signature(), "tstz");
}

TEST_F(SubstraitTypeTest, date_test) {
  auto type = SubstraitType::decode("date");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kDate);
  ASSERT_EQ(type->signature(), "date");
}

TEST_F(SubstraitTypeTest, time_test) {
  auto type = SubstraitType::decode("time");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kTime);
  ASSERT_EQ(type->signature(), "time");
}

TEST_F(SubstraitTypeTest, interval_day_test) {
  auto type = SubstraitType::decode("interval_day");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kIntervalDay);
  ASSERT_EQ(type->signature(), "iday");
}

TEST_F(SubstraitTypeTest, interval_year_test) {
  auto type = SubstraitType::decode("interval_year");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kIntervalYear);
  ASSERT_EQ(type->signature(), "iyear");
}

TEST_F(SubstraitTypeTest, uuid_test) {
  auto type = SubstraitType::decode("uuid");
  ASSERT_TRUE(type->kind() == SubstraitTypeKind::kUuid);
  ASSERT_EQ(type->signature(), "uuid");
}
//    SUBSTRAIT_SCALAR_TYPE_MAPPING(kUuid),

TEST_F(SubstraitTypeTest, fromBool) {
  auto boolType = SubstraitType::decode("i8");
  ASSERT_TRUE(boolType->kind() == SubstraitTypeKind::kI8);
  auto unknown = SubstraitType::decode("unknown");
  ASSERT_TRUE(unknown->isUnknown());
  auto any = SubstraitType::decode("any1");
  ASSERT_TRUE(any->isWildcard());
}

TEST_F(SubstraitTypeTest, fromVelox) {
  auto boolType = SubstraitType::fromVelox(BOOLEAN());
  ASSERT_TRUE(boolType->kind() == SubstraitTypeKind::kBool);
}