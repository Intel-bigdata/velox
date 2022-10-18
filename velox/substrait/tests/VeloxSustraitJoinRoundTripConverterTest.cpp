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

#include <folly/Random.h>
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
  static std::vector<std::string> concat(
      const std::vector<std::string>& a,
      const std::vector<std::string>& b) {
    std::vector<std::string> result;
    result.insert(result.end(), a.begin(), a.end());
    result.insert(result.end(), b.begin(), b.end());
    return result;
  }

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

  void testJoin(
      const std::vector<TypePtr>& keyTypes,
      int32_t leftSize,
      int32_t rightSize,
      const core::JoinType joinType,
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
                        .values({leftBatch}, true)
                        .hashJoin(
                            makeKeyNames(keyTypes.size(), "t_"),
                            makeKeyNames(keyTypes.size(), "u_"),
                            PlanBuilder(planNodeIdGenerator)
                                .values({rightBatch}, true)
                                .planNode(),
                            filter,
                            concat(leftType->names(), rightType->names()),
                            joinType)
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

TEST_F(VeloxSubstraitJoinRoundTripConverterTest, innerJoin) {
  testJoin(
      {BIGINT()},
      16000,
      15000,
      core::JoinType::kInner,
      "SELECT t_k0, t_data, u_k0, u_data FROM "
      "  t, u "
      "  WHERE t_k0 = u_k0 AND ((t_k0 % 100) + (u_k0 % 100)) % 40 < 20",
      "((t_k0 % 100) + (u_k0 % 100)) % 40 < 20");

  testJoin(
      {BIGINT()},
      16000,
      15000,
      core::JoinType::kLeft,
      "SELECT t_k0, t_data, u_k0, u_data FROM "
      "  t let join  u  on t_k0= u_k0"
      "  WHERE   ((t_k0 % 100) + (u_k0 % 100)) % 40 < 20",
      "((t_k0 % 100) + (u_k0 % 100)) % 40 < 20");
}
