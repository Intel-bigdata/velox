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

#include <folly/init/Init.h>
#include "velox/dwio/common/tests/utils/BatchMaker.h"
#include "velox/exec/PlanNodeStats.h"
#include "velox/exec/tests/utils/PlanBuilder.h"

#include "velox/exec/tests/utils/OperatorTestBase.h"
#include "velox/substrait/SubstraitToVeloxPlan.h"
#include "velox/substrait/VeloxToSubstraitPlan.h"

using namespace facebook::velox;
using namespace facebook::velox::test;
using namespace facebook::velox::exec::test;
using namespace facebook::velox::substrait;

class VeloxSubstraitJoinRoundTripConverterTest : public OperatorTestBase {
 protected:
  static std::vector<std::string> makeKeyNames(
      int cnt,
      const std::string& prefix) {
    std::vector<std::string> names;
    for (int i = 0; i < cnt; ++i) {
      names.push_back(fmt::format("{}k{}", prefix, i));
    }
    return names;
  }

  static RowTypePtr makeRowType(
      const std::vector<TypePtr>& keyTypes,
      const std::string& namePrefix) {
    std::vector<std::string> names = makeKeyNames(keyTypes.size(), namePrefix);
    names.push_back(fmt::format("{}data", namePrefix));

    std::vector<TypePtr> types = keyTypes;
    types.push_back(VARCHAR());

    return ROW(std::move(names), std::move(types));
  }

  static std::vector<std::string> concat(
      const std::vector<std::string>& a,
      const std::vector<std::string>& b) {
    std::vector<std::string> result;
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
  }

  void testJoin(
      const std::vector<TypePtr>& keyTypes,
      int32_t leftSize,
      int32_t rightSize,
      const std::string& referenceQuery,
      const std::string& filter = "") {
    auto leftType = makeRowType(keyTypes, "t_");
    auto rightType = makeRowType(keyTypes, "u_");

    auto leftBatch = std::dynamic_pointer_cast<RowVector>(
        BatchMaker::createBatch(leftType, leftSize, *pool_));
    auto rightBatch = std::dynamic_pointer_cast<RowVector>(
        BatchMaker::createBatch(rightType, rightSize, *pool_));

    auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();

    auto planNode = PlanBuilder(planNodeIdGenerator)
                        .values({leftBatch})
                        .hashJoin(
                            makeKeyNames(keyTypes.size(), "t_"),
                            makeKeyNames(keyTypes.size(), "u_"),
                            PlanBuilder(planNodeIdGenerator)
                                .values({rightBatch})
                                .planNode(),
                            filter,
                            concat(leftType->names(), rightType->names()))
                        .planNode();

    createDuckDbTable("t", {leftBatch});
    createDuckDbTable("u", {rightBatch});

    assertPlanConversion(planNode, referenceQuery);
  }

  void assertPlanConversion(
      const std::shared_ptr<const core::PlanNode>& plan,
      const std::string& duckDbSql) {
    assertQuery(plan, duckDbSql);

    // Convert Velox Plan to Substrait Plan.
    google::protobuf::Arena arena;
    auto substraitPlan = veloxConvertor_->toSubstrait(arena, plan);

    // Convert Substrait Plan to the same Velox Plan.
    auto samePlan =
        substraitConverter_->toVeloxPlan(substraitPlan, pool_.get());

    // Assert velox again.
    assertQuery(samePlan, duckDbSql);
  }

  std::shared_ptr<VeloxToSubstraitPlanConvertor> veloxConvertor_ =
      std::make_shared<VeloxToSubstraitPlanConvertor>();

  std::shared_ptr<SubstraitVeloxPlanConverter> substraitConverter_ =
      std::make_shared<SubstraitVeloxPlanConverter>();
};

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, bigintArray) {
  testJoin(
      {BIGINT()},
      16000,
      15000,
      "SELECT t_k0, t_data, u_k0, u_data FROM "
      "  t, u "
      "  WHERE t_k0 = u_k0");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, emptyBuild) {
  testJoin(
      {BIGINT()},
      16000,
      0,
      "SELECT t_k0, t_data, u_k0, u_data FROM "
      "  t, u "
      "  WHERE t_k0 = u_k0");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, normalizedKey) {
  testJoin(
      {INTEGER(), INTEGER(), INTEGER()},
      16000,
      15000,
      "SELECT t_k0, t_k1, t_data, u_k0, u_k1, u_data FROM "
      "  t, u "
      "  WHERE t_k0 = u_k0 AND t_k1 = u_k1");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, filter) {
  testJoin(
      {BIGINT()},
      16000,
      15000,
      "SELECT t_k0, t_data, u_k0, u_data FROM "
      "  t, u "
      "  WHERE t_k0 = u_k0 AND ((t_k0 % 100) + (u_k0 % 100)) % 40 < 20",
      "((t_k0 % 100) + (u_k0 % 100)) % 40 < 20");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, leftJoin) {
  auto leftVectors = {
      makeRowVector({
          makeFlatVector<int32_t>({1, 2, 3}),
          makeNullableFlatVector<int32_t>({10, std::nullopt, 30}),
      }),
      makeRowVector({
          makeFlatVector<int32_t>({1, 2, 3}),
          makeNullableFlatVector<int32_t>({std::nullopt, 20, 30}),
      })};
  auto rightVectors = {
      makeRowVector({makeFlatVector<int32_t>({1, 2, 10})}),
  };

  createDuckDbTable("t", leftVectors);
  createDuckDbTable("u", rightVectors);

  auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();

  auto buildSide = PlanBuilder(planNodeIdGenerator)
                       .values(rightVectors)
                       .project({"c0 AS u_c0"})
                       .planNode();

  auto plan = PlanBuilder(planNodeIdGenerator)
                  .values(leftVectors)
                  .hashJoin(
                      {"c0"},
                      {"u_c0"},
                      buildSide,
                      "c1 + u_c0 > 0",
                      {"c0", "u_c0"},
                      core::JoinType::kLeft)
                  .planNode();

  assertPlanConversion(
      plan,
      "SELECT t.c0,u.c0  FROM t LEFT JOIN u ON (t.c0 = u.c0 AND t.c1 + u.c0 > 0)");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, rightJoin) {
  auto leftVectors = {
      makeRowVector({
          makeFlatVector<int32_t>({1, 2, 3, 4, 5}),
          makeNullableFlatVector<int32_t>(
              {10, std::nullopt, 30, std::nullopt, 50}),
      }),
      makeRowVector({
          makeFlatVector<int32_t>({1, 2, 3, 4, 5}),
          makeNullableFlatVector<int32_t>(
              {std::nullopt, 20, 30, std::nullopt, 50}),
      })};
  auto rightVectors = {
      makeRowVector({makeFlatVector<int32_t>({1, 2, 10, 30, 40})}),
  };

  createDuckDbTable("t", leftVectors);
  createDuckDbTable("u", rightVectors);

  auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();

  auto buildSide = PlanBuilder(planNodeIdGenerator)
                       .values(rightVectors)
                       .project({"c0 AS u_c0"})
                       .planNode();

  auto plan = PlanBuilder(planNodeIdGenerator)
                  .values(leftVectors)
                  .hashJoin(
                      {"c0"},
                      {"u_c0"},
                      buildSide,
                      "c1 + u_c0 > 0",
                      {"c0", "c1"},
                      core::JoinType::kRight)
                  .planNode();

  assertPlanConversion(
      plan,
      "SELECT t.c0, t.c1 FROM t RIGHT JOIN u ON (t.c0 = u.c0 AND t.c1 + u.c0 > 0)");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, leftSemiJoin) {
  auto leftVectors = makeRowVector({
      makeFlatVector<int32_t>(
          1'234, [](auto row) { return row % 11; }, nullEvery(13)),
      makeFlatVector<int32_t>(1'234, [](auto row) { return row; }),
  });

  auto rightVectors = makeRowVector({
      makeFlatVector<int32_t>(
          123, [](auto row) { return row % 5; }, nullEvery(7)),
  });

  createDuckDbTable("t", {leftVectors});
  createDuckDbTable("u", {rightVectors});

  auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();
  auto op = PlanBuilder(planNodeIdGenerator)
                .values({leftVectors})
                .hashJoin(
                    {"c0"},
                    {"u_c0"},
                    PlanBuilder(planNodeIdGenerator)
                        .values({rightVectors})
                        .project({"c0 as u_c0"})
                        .planNode(),
                    "",
                    {"c1"},
                    core::JoinType::kLeftSemi)
                .planNode();

  assertPlanConversion(
      op, "SELECT t.c1 FROM t WHERE t.c0 IN (SELECT c0 FROM u)");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, fullJoin) {
  // Left side keys are [0, 1, 2,..10].
  auto leftVectors = {
      makeRowVector({
          makeFlatVector<int32_t>(
              2'222, [](auto row) { return row % 11; }, nullEvery(13)),
          makeFlatVector<int32_t>(2'222, [](auto row) { return row; }),
      }),
      makeRowVector({
          makeFlatVector<int32_t>(
              2'222, [](auto row) { return (row + 3) % 11; }, nullEvery(13)),
          makeFlatVector<int32_t>(2'222, [](auto row) { return row; }),
      }),
  };

  // Right side keys are [-3, -2, -1, 0, 1, 2, 3].
  auto rightVectors = makeRowVector({
      makeFlatVector<int32_t>(
          123, [](auto row) { return -3 + row % 7; }, nullEvery(11)),
      makeFlatVector<int32_t>(
          123, [](auto row) { return -111 + row * 2; }, nullEvery(13)),
  });

  createDuckDbTable("t", leftVectors);
  createDuckDbTable("u", {rightVectors});

  auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();

  auto buildSide = PlanBuilder(planNodeIdGenerator)
                       .values({rightVectors})
                       .project({"c0 AS u_c0", "c1 AS u_c1"})
                       .planNode();

  auto op = PlanBuilder(planNodeIdGenerator)
                .values(leftVectors)
                .hashJoin(
                    {"c0"},
                    {"u_c0"},
                    buildSide,
                    "",
                    {"c0", "c1", "u_c1"},
                    core::JoinType::kFull)
                .planNode();

  assertPlanConversion(
      op, "SELECT t.c0, t.c1, u.c1 FROM t FULL OUTER JOIN u ON t.c0 = u.c0");
}

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, antiJoin) {
  auto leftVectors = makeRowVector({
      makeFlatVector<int32_t>(
          1'000, [](auto row) { return row % 11; }, nullEvery(13)),
      makeFlatVector<int32_t>(1'000, [](auto row) { return row; }),
  });

  auto rightVectors = makeRowVector({
      makeFlatVector<int32_t>(
          1'234, [](auto row) { return row % 5; }, nullEvery(7)),
  });

  createDuckDbTable("t", {leftVectors});
  createDuckDbTable("u", {rightVectors});

  auto planNodeIdGenerator = std::make_shared<PlanNodeIdGenerator>();
  auto op = PlanBuilder(planNodeIdGenerator)
                .values({leftVectors})
                .hashJoin(
                    {"c0"},
                    {"c0"},
                    PlanBuilder(planNodeIdGenerator)
                        .values({rightVectors})
                        .filter("c0 IS NOT NULL")
                        .planNode(),
                    "",
                    {"c1"},
                    core::JoinType::kAnti)
                .planNode();

  assertQuery(
      op,
      "SELECT t.c1 FROM t WHERE t.c0 NOT IN (SELECT c0 FROM u WHERE c0 IS NOT NULL)");
}