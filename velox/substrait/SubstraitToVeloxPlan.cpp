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

#include "velox/substrait/SubstraitToVeloxPlan.h"
#include "velox/substrait/TypeUtils.h"
#include "velox/substrait/VariantToVectorConverter.h"
#include "velox/type/Type.h"

namespace facebook::velox::substrait {
namespace {
core::AggregationNode::Step toAggregationStep(
    const ::substrait::AggregateRel& sAgg) {
  if (sAgg.measures().size() == 0) {
    // When only groupings exist, set the phase to be Single.
    return core::AggregationNode::Step::kSingle;
  }

  // Use the first measure to set aggregation phase.
  const auto& firstMeasure = sAgg.measures()[0];
  const auto& aggFunction = firstMeasure.measure();
  switch (aggFunction.phase()) {
    case ::substrait::AGGREGATION_PHASE_INITIAL_TO_INTERMEDIATE:
      return core::AggregationNode::Step::kPartial;
    case ::substrait::AGGREGATION_PHASE_INTERMEDIATE_TO_INTERMEDIATE:
      return core::AggregationNode::Step::kIntermediate;
    case ::substrait::AGGREGATION_PHASE_INTERMEDIATE_TO_RESULT:
      return core::AggregationNode::Step::kFinal;
    case ::substrait::AGGREGATION_PHASE_INITIAL_TO_RESULT:
      return core::AggregationNode::Step::kSingle;
    default:
      VELOX_FAIL("Aggregate phase is not supported.");
  }
}
} // namespace

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::AggregateRel& aggRel) {
  core::PlanNodePtr childNode;
  if (aggRel.has_input()) {
    childNode = toVeloxPlan(aggRel.input());
  } else {
    VELOX_FAIL("Child Rel is expected in AggregateRel.");
  }
  core::AggregationNode::Step aggStep = toAggregationStep(aggRel);
  const auto& inputType = childNode->outputType();
  std::vector<core::FieldAccessTypedExprPtr> veloxGroupingExprs;

  // Get the grouping expressions.
  for (const auto& grouping : aggRel.groupings()) {
    for (const auto& groupingExpr : grouping.grouping_expressions()) {
      // Velox's groupings are limited to be Field.
      veloxGroupingExprs.emplace_back(
          exprConverter_->toVeloxExpr(groupingExpr.selection(), inputType));
    }
  }

  // Parse measures and get the aggregate expressions.
  // Each measure represents one aggregate expression.
  std::vector<core::CallTypedExprPtr> aggExprs;
  aggExprs.reserve(aggRel.measures().size());
  std::vector<core::FieldAccessTypedExprPtr> aggregateMasks;
  aggregateMasks.reserve(aggRel.measures().size());

  for (const auto& measure : aggRel.measures()) {
    core::FieldAccessTypedExprPtr aggregateMask;
    ::substrait::Expression substraitAggMask = measure.filter();
    // Get Aggregation Masks.
    if (measure.has_filter()) {
      if (substraitAggMask.ByteSizeLong() == 0) {
        aggregateMask = {};
      } else {
        aggregateMask =
            std::dynamic_pointer_cast<const core::FieldAccessTypedExpr>(
                exprConverter_->toVeloxExpr(substraitAggMask, inputType));
      }
      aggregateMasks.push_back(aggregateMask);
    }

    const auto& aggFunction = measure.measure();
    auto funcName = substraitParser_->findVeloxFunction(
        functionMap_, aggFunction.function_reference());
    std::vector<core::TypedExprPtr> aggParams;
    aggParams.reserve(aggFunction.arguments().size());
    for (const auto& arg : aggFunction.arguments()) {
      aggParams.emplace_back(
          exprConverter_->toVeloxExpr(arg.value(), inputType));
    }
    auto aggVeloxType = toVeloxType(
        substraitParser_->parseType(aggFunction.output_type())->type);
    auto aggExpr = std::make_shared<const core::CallTypedExpr>(
        aggVeloxType, std::move(aggParams), funcName);
    aggExprs.emplace_back(aggExpr);
  }

  bool ignoreNullKeys = false;
  std::vector<core::FieldAccessTypedExprPtr> preGroupingExprs;

  // Get the output names of Aggregation.
  std::vector<std::string> aggOutNames;
  aggOutNames.reserve(aggRel.measures().size());
  for (int idx = veloxGroupingExprs.size();
       idx < veloxGroupingExprs.size() + aggRel.measures().size();
       idx++) {
    aggOutNames.emplace_back(substraitParser_->makeNodeName(planNodeId_, idx));
  }

  // Create Aggregate node.
  return std::make_shared<core::AggregationNode>(
      nextPlanNodeId(),
      aggStep,
      veloxGroupingExprs,
      preGroupingExprs,
      aggOutNames,
      aggExprs,
      aggregateMasks,
      ignoreNullKeys,
      childNode);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::ProjectRel& projectRel) {
  core::PlanNodePtr childNode;
  if (projectRel.has_input()) {
    childNode = toVeloxPlan(projectRel.input());
  } else {
    VELOX_FAIL("Child Rel is expected in ProjectRel.");
  }

  // Construct Velox Expressions.
  auto projectExprs = projectRel.expressions();
  std::vector<std::string> projectNames;
  std::vector<core::TypedExprPtr> expressions;
  projectNames.reserve(projectExprs.size());
  expressions.reserve(projectExprs.size());

  const auto& inputType = childNode->outputType();
  int colIdx = 0;
  for (const auto& expr : projectExprs) {
    expressions.emplace_back(exprConverter_->toVeloxExpr(expr, inputType));
    projectNames.emplace_back(
        substraitParser_->makeNodeName(planNodeId_, colIdx));
    colIdx += 1;
  }

  return std::make_shared<core::ProjectNode>(
      nextPlanNodeId(),
      std::move(projectNames),
      std::move(expressions),
      childNode);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::SortRel& sortRel) {
  core::PlanNodePtr childNode;
  if (sortRel.has_input()) {
    childNode = toVeloxPlan(sortRel.input());
  } else {
    VELOX_FAIL("Child Rel is expected in SortRel.");
  }

  const auto& inputType = childNode->outputType();

  std::vector<core::FieldAccessTypedExprPtr> sortingKeys;
  std::vector<core::SortOrder> sortingOrders;

  const auto& sorts = sortRel.sorts();
  sortingKeys.reserve(sorts.size());
  sortingOrders.reserve(sorts.size());

  for (const auto& sort : sorts) {
    switch (sort.direction()) {
      case ::substrait::SortField_SortDirection_SORT_DIRECTION_ASC_NULLS_FIRST:
        sortingOrders.emplace_back(core::kAscNullsFirst);
        break;
      case ::substrait::SortField_SortDirection_SORT_DIRECTION_ASC_NULLS_LAST:
        sortingOrders.emplace_back(core::kAscNullsLast);
        break;
      case ::substrait::SortField_SortDirection_SORT_DIRECTION_DESC_NULLS_FIRST:
        sortingOrders.emplace_back(core::kDescNullsFirst);
        break;
      case ::substrait::SortField_SortDirection_SORT_DIRECTION_DESC_NULLS_LAST:
        sortingOrders.emplace_back(core::kDescNullsLast);
        break;
      default:
        VELOX_FAIL("Sort direction is not support in SortRel");
    }

    if (sort.has_expr()) {
      auto expression = exprConverter_->toVeloxExpr(sort.expr(), inputType);
      auto expr_field =
          dynamic_cast<const core::FieldAccessTypedExpr*>(expression.get());
      VELOX_CHECK(
          expr_field != nullptr,
          " the sorting key in Sort Operator only support field")

      sortingKeys.emplace_back(
          std::dynamic_pointer_cast<const core::FieldAccessTypedExpr>(
              expression));
    }
  }

  return std::make_shared<core::OrderByNode>(
      nextPlanNodeId(), sortingKeys, sortingOrders, false, childNode);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::FilterRel& filterRel) {
  core::PlanNodePtr childNode;
  if (filterRel.has_input()) {
    childNode = toVeloxPlan(filterRel.input());
  } else {
    VELOX_FAIL("Child Rel is expected in FilterRel.");
  }

  const auto& inputType = childNode->outputType();
  const auto& sExpr = filterRel.condition();

  return std::make_shared<core::FilterNode>(
      nextPlanNodeId(),
      exprConverter_->toVeloxExpr(sExpr, inputType),
      childNode);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::FetchRel& fetchRel) {
  core::PlanNodePtr childNode;
  if (fetchRel.has_input()) {
    childNode = toVeloxPlan(fetchRel.input());
  } else {
    VELOX_FAIL("Child Rel is expected in FetchRel.");
  }

  return std::make_shared<core::LimitNode>(
      nextPlanNodeId(),
      (int32_t)fetchRel.offset(),
      (int32_t)fetchRel.count(),
      false /*isPartial*/,
      childNode);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::ReadRel& readRel,
    std::shared_ptr<SplitInfo>& splitInfo) {
  // Get output names and types.
  std::vector<std::string> colNameList;
  std::vector<TypePtr> veloxTypeList;
  if (readRel.has_base_schema()) {
    const auto& baseSchema = readRel.base_schema();
    colNameList.reserve(baseSchema.names().size());
    for (const auto& name : baseSchema.names()) {
      colNameList.emplace_back(name);
    }
    auto substraitTypeList = substraitParser_->parseNamedStruct(baseSchema);
    veloxTypeList.reserve(substraitTypeList.size());
    for (const auto& substraitType : substraitTypeList) {
      veloxTypeList.emplace_back(toVeloxType(substraitType->type));
    }
  }

  // Parse local files
  if (readRel.has_local_files()) {
    using SubstraitFileFormatCase =
        ::substrait::ReadRel_LocalFiles_FileOrFiles::FileFormatCase;
    const auto& fileList = readRel.local_files().items();
    splitInfo->paths.reserve(fileList.size());
    splitInfo->starts.reserve(fileList.size());
    splitInfo->lengths.reserve(fileList.size());
    for (const auto& file : fileList) {
      // Expect all files to share the same index.
      splitInfo->partitionIndex = file.partition_index();
      splitInfo->paths.emplace_back(file.uri_file());
      splitInfo->starts.emplace_back(file.start());
      splitInfo->lengths.emplace_back(file.length());
      switch (file.file_format_case()) {
        case SubstraitFileFormatCase::kOrc:
          splitInfo->format = dwio::common::FileFormat::DWRF;
          break;
        case SubstraitFileFormatCase::kParquet:
          splitInfo->format = dwio::common::FileFormat::PARQUET;
          break;
        default:
          splitInfo->format = dwio::common::FileFormat::UNKNOWN;
      }
    }
  }

  // Do not hard-code connector ID and allow for connectors other than Hive.
  static const std::string kHiveConnectorId = "test-hive";

  // Velox requires Filter Pushdown must being enabled.
  bool filterPushdownEnabled = true;
  std::shared_ptr<connector::hive::HiveTableHandle> tableHandle;
  if (!readRel.has_filter()) {
    tableHandle = std::make_shared<connector::hive::HiveTableHandle>(
        kHiveConnectorId,
        "hive_table",
        filterPushdownEnabled,
        connector::hive::SubfieldFilters{},
        nullptr);
  } else {
    connector::hive::SubfieldFilters filters =
        toVeloxFilter(colNameList, veloxTypeList, readRel.filter());
    tableHandle = std::make_shared<connector::hive::HiveTableHandle>(
        kHiveConnectorId,
        "hive_table",
        filterPushdownEnabled,
        std::move(filters),
        nullptr);
  }

  // Get assignments and out names.
  std::vector<std::string> outNames;
  outNames.reserve(colNameList.size());
  std::unordered_map<std::string, std::shared_ptr<connector::ColumnHandle>>
      assignments;
  for (int idx = 0; idx < colNameList.size(); idx++) {
    auto outName = substraitParser_->makeNodeName(planNodeId_, idx);
    assignments[outName] = std::make_shared<connector::hive::HiveColumnHandle>(
        colNameList[idx],
        connector::hive::HiveColumnHandle::ColumnType::kRegular,
        veloxTypeList[idx]);
    outNames.emplace_back(outName);
  }
  auto outputType = ROW(std::move(outNames), std::move(veloxTypeList));

  if (readRel.has_virtual_table()) {
    return toVeloxPlan(readRel, outputType);

  } else {
    auto tableScanNode = std::make_shared<core::TableScanNode>(
        nextPlanNodeId(), outputType, tableHandle, assignments);
    return tableScanNode;
  }
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::ReadRel& readRel,
    const RowTypePtr& type) {
  ::substrait::ReadRel_VirtualTable readVirtualTable = readRel.virtual_table();
  int64_t numVectors = readVirtualTable.values_size();
  int64_t numColumns = type->size();
  int64_t valueFieldNums =
      readVirtualTable.values(numVectors - 1).fields_size();
  std::vector<RowVectorPtr> vectors;
  vectors.reserve(numVectors);

  int64_t batchSize;
  // For the empty vectors, eg,vectors = makeRowVector(ROW({}, {}), 1).
  if (numColumns == 0) {
    batchSize = 1;
  } else {
    batchSize = valueFieldNums / numColumns;
  }

  for (int64_t index = 0; index < numVectors; ++index) {
    std::vector<VectorPtr> children;
    ::substrait::Expression_Literal_Struct rowValue =
        readRel.virtual_table().values(index);
    auto fieldSize = rowValue.fields_size();
    VELOX_CHECK_EQ(fieldSize, batchSize * numColumns);

    for (int64_t col = 0; col < numColumns; ++col) {
      const TypePtr& outputChildType = type->childAt(col);
      std::vector<variant> batchChild;
      batchChild.reserve(batchSize);
      for (int64_t batchId = 0; batchId < batchSize; batchId++) {
        // each value in the batch
        auto fieldIdx = col * batchSize + batchId;
        ::substrait::Expression_Literal field = rowValue.fields(fieldIdx);

        auto expr = exprConverter_->toVeloxExpr(field);
        if (auto constantExpr =
                std::dynamic_pointer_cast<const core::ConstantTypedExpr>(
                    expr)) {
          if (!constantExpr->hasValueVector()) {
            batchChild.emplace_back(constantExpr->value());
          } else {
            VELOX_UNSUPPORTED(
                "Values node with complex type values is not supported yet");
          }
        } else {
          VELOX_FAIL("Expected constant expression");
        }
      }
      children.emplace_back(
          setVectorFromVariants(outputChildType, batchChild, pool_));
    }

    vectors.emplace_back(
        std::make_shared<RowVector>(pool_, type, nullptr, batchSize, children));
  }

  return std::make_shared<core::ValuesNode>(nextPlanNodeId(), vectors);
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::Rel& rel) {
  if (rel.has_aggregate()) {
    return toVeloxPlan(rel.aggregate());
  }
  if (rel.has_project()) {
    return toVeloxPlan(rel.project());
  }
  if (rel.has_filter()) {
    return toVeloxPlan(rel.filter());
  }
  if (rel.has_read()) {
    auto splitInfo = std::make_shared<SplitInfo>();

    auto planNode = toVeloxPlan(rel.read(), splitInfo);
    splitInfoMap_[planNode->id()] = splitInfo;
    return planNode;
  }
  if (rel.has_join()) {
    return toVeloxPlan(rel.join());
  }
  if (rel.has_fetch()) {
    return toVeloxPlan(rel.fetch());
  }
  if (rel.has_sort()) {
    return toVeloxPlan(rel.sort());
  }

  VELOX_NYI("Substrait conversion not supported for Rel.");
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::RelRoot& root) {
  // TODO: Use the names as the output names for the whole computing.
  const auto& names = root.names();
  if (root.has_input()) {
    const auto& rel = root.input();
    return toVeloxPlan(rel);
  }
  VELOX_FAIL("Input is expected in RelRoot.");
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::Plan& substraitPlan) {
  VELOX_CHECK(
      checkTypeExtension(substraitPlan),
      "The type extension only have unknown type.")
  // Construct the function map based on the Substrait representation.
  constructFunctionMap(substraitPlan);

  // Construct the expression converter.
  exprConverter_ =
      std::make_shared<SubstraitVeloxExprConverter>(pool_, functionMap_);

  // In fact, only one RelRoot or Rel is expected here.
  VELOX_CHECK_EQ(substraitPlan.relations_size(), 1);
  const auto& rel = substraitPlan.relations(0);
  if (rel.has_root()) {
    return toVeloxPlan(rel.root());
  }
  if (rel.has_rel()) {
    return toVeloxPlan(rel.rel());
  }

  VELOX_FAIL("RelRoot or Rel is expected in Plan.");
}

std::string SubstraitVeloxPlanConverter::nextPlanNodeId() {
  auto id = fmt::format("{}", planNodeId_);
  planNodeId_++;
  return id;
}

// This class contains the needed infos for Filter Pushdown.
// TODO: Support different types here.
class FilterInfo {
 public:
  // Used to set the left bound.
  void setLeft(double left, bool isExclusive) {
    left_ = left;
    leftExclusive_ = isExclusive;
    if (!isInitialized_) {
      isInitialized_ = true;
    }
  }

  // Used to set the right bound.
  void setRight(double right, bool isExclusive) {
    right_ = right;
    rightExclusive_ = isExclusive;
    if (!isInitialized_) {
      isInitialized_ = true;
    }
  }

  // Will fordis Null value if called once.
  void forbidsNull() {
    nullAllowed_ = false;
    if (!isInitialized_) {
      isInitialized_ = true;
    }
  }

  // Return the initialization status.
  bool isInitialized() {
    return isInitialized_ ? true : false;
  }

  // The left bound.
  std::optional<double> left_ = std::nullopt;
  // The right bound.
  std::optional<double> right_ = std::nullopt;
  // The Null allowing.
  bool nullAllowed_ = true;
  // If true, left bound will be exclusive.
  bool leftExclusive_ = false;
  // If true, right bound will be exclusive.
  bool rightExclusive_ = false;

 private:
  bool isInitialized_ = false;
};

connector::hive::SubfieldFilters SubstraitVeloxPlanConverter::toVeloxFilter(
    const std::vector<std::string>& inputNameList,
    const std::vector<TypePtr>& inputTypeList,
    const ::substrait::Expression& substraitFilter) {
  connector::hive::SubfieldFilters filters;
  // A map between the column index and the FilterInfo for that column.
  std::unordered_map<int, std::shared_ptr<FilterInfo>> colInfoMap;
  for (int idx = 0; idx < inputNameList.size(); idx++) {
    colInfoMap[idx] = std::make_shared<FilterInfo>();
  }

  std::vector<::substrait::Expression_ScalarFunction> scalarFunctions;
  flattenConditions(substraitFilter, scalarFunctions);
  // Construct the FilterInfo for the related column.
  for (const auto& scalarFunction : scalarFunctions) {
    auto filterNameSpec = substraitParser_->findFunctionSpec(
        functionMap_, scalarFunction.function_reference());
    auto filterName = getNameBeforeDelimiter(filterNameSpec, ":");
    int32_t colIdx;
    // TODO: Add different types' support here.
    double val;
    for (auto& arg : scalarFunction.arguments()) {
      auto argExpr = arg.value();
      auto typeCase = argExpr.rex_type_case();
      switch (typeCase) {
        case ::substrait::Expression::RexTypeCase::kSelection: {
          auto sel = argExpr.selection();
          // TODO: Only direct reference is considered here.
          auto dRef = sel.direct_reference();
          colIdx = substraitParser_->parseReferenceSegment(dRef);
          break;
        }
        case ::substrait::Expression::RexTypeCase::kLiteral: {
          auto sLit = argExpr.literal();
          // TODO: Only double is considered here.
          val = sLit.fp64();
          break;
        }
        default:
          VELOX_NYI(
              "Substrait conversion not supported for arg type '{}'", typeCase);
      }
    }
    if (filterName == "is_not_null") {
      colInfoMap[colIdx]->forbidsNull();
    } else if (filterName == "gte") {
      colInfoMap[colIdx]->setLeft(val, false);
    } else if (filterName == "gt") {
      colInfoMap[colIdx]->setLeft(val, true);
    } else if (filterName == "lte") {
      colInfoMap[colIdx]->setRight(val, false);
    } else if (filterName == "lt") {
      colInfoMap[colIdx]->setRight(val, true);
    } else {
      VELOX_NYI(
          "Substrait conversion not supported for filter name '{}'",
          filterName);
    }
  }

  // Construct the Filters.
  for (int idx = 0; idx < inputNameList.size(); idx++) {
    auto filterInfo = colInfoMap[idx];
    double leftBound;
    double rightBound;
    bool leftUnbounded = true;
    bool rightUnbounded = true;
    bool leftExclusive = false;
    bool rightExclusive = false;
    if (filterInfo->isInitialized()) {
      if (filterInfo->left_) {
        leftUnbounded = false;
        leftBound = filterInfo->left_.value();
        leftExclusive = filterInfo->leftExclusive_;
      }
      if (filterInfo->right_) {
        rightUnbounded = false;
        rightBound = filterInfo->right_.value();
        rightExclusive = filterInfo->rightExclusive_;
      }
      bool nullAllowed = filterInfo->nullAllowed_;
      filters[common::Subfield(inputNameList[idx])] =
          std::make_unique<common::DoubleRange>(
              leftBound,
              leftUnbounded,
              leftExclusive,
              rightBound,
              rightUnbounded,
              rightExclusive,
              nullAllowed);
    }
  }
  return filters;
}

void SubstraitVeloxPlanConverter::flattenConditions(
    const ::substrait::Expression& substraitFilter,
    std::vector<::substrait::Expression_ScalarFunction>& scalarFunctions) {
  auto typeCase = substraitFilter.rex_type_case();
  switch (typeCase) {
    case ::substrait::Expression::RexTypeCase::kScalarFunction: {
      auto sFunc = substraitFilter.scalar_function();
      auto filterNameSpec = substraitParser_->findFunctionSpec(
          functionMap_, sFunc.function_reference());
      // TODO: Only and relation is supported here.
      if (getNameBeforeDelimiter(filterNameSpec, ":") == "and") {
        for (const auto& sCondition : sFunc.arguments()) {
          flattenConditions(sCondition.value(), scalarFunctions);
        }
      } else {
        scalarFunctions.emplace_back(sFunc);
      }
      break;
    }
    default:
      VELOX_NYI("GetFlatConditions not supported for type '{}'", typeCase);
  }
}

void SubstraitVeloxPlanConverter::constructFunctionMap(
    const ::substrait::Plan& substraitPlan) {
  // Construct the function map based on the Substrait representation.
  for (const auto& sExtension : substraitPlan.extensions()) {
    if (!sExtension.has_extension_function()) {
      continue;
    }
    const auto& sFmap = sExtension.extension_function();
    auto id = sFmap.function_anchor();
    auto name = sFmap.name();
    functionMap_[id] = name;
  }
}

bool SubstraitVeloxPlanConverter::checkTypeExtension(
    const ::substrait::Plan& substraitPlan) {
  for (const auto& sExtension : substraitPlan.extensions()) {
    if (!sExtension.has_extension_type()) {
      continue;
    }

    // Only support UNKNOWN type in UserDefined type extension.
    if (sExtension.extension_type().name() != "UNKNOWN") {
      return false;
    }
  }
  return true;
}

const std::string& SubstraitVeloxPlanConverter::findFunction(
    uint64_t id) const {
  return substraitParser_->findFunctionSpec(functionMap_, id);
}

void SubstraitVeloxPlanConverter::extractJoinKeys(
    const ::substrait::Expression& joinExpression,
    std::vector<const ::substrait::Expression::FieldReference*>& leftExprs,
    std::vector<const ::substrait::Expression::FieldReference*>& rightExprs) {
  std::vector<const ::substrait::Expression*> expressions;
  expressions.push_back(&joinExpression);
  while (!expressions.empty()) {
    auto visited = expressions.back();
    expressions.pop_back();
    if (visited->rex_type_case() ==
        ::substrait::Expression::RexTypeCase::kScalarFunction) {
      auto sFunc = visited->scalar_function();
      auto funcName = substraitParser_->findFunctionSpec(
          functionMap_, sFunc.function_reference());
      const auto& args = visited->scalar_function().arguments();
      if (funcName == "and") {
        expressions.push_back(&args[0].value());
        expressions.push_back(&args[1].value());
      } else if (funcName == "eq") {
        VELOX_CHECK(std::all_of(
            args.cbegin(),
            args.cend(),
            [](const ::substrait::FunctionArgument& arg) {
              return arg.value().has_selection();
            }));
        leftExprs.push_back(&args[0].value().selection());
        rightExprs.push_back(&args[1].value().selection());
      } else {
        VELOX_NYI("Join condition {} not supported.", funcName);
      }
    } else {
      VELOX_FAIL(
          "Unable to parse from join expression: {}",
          joinExpression.DebugString());
    }
  }
}

core::PlanNodePtr SubstraitVeloxPlanConverter::toVeloxPlan(
    const ::substrait::JoinRel& sJoin) {
  if (!sJoin.has_left()) {
    VELOX_FAIL("Left Rel is expected in JoinRel.");
  }
  if (!sJoin.has_right()) {
    VELOX_FAIL("Right Rel is expected in JoinRel.");
  }

  auto leftNode = toVeloxPlan(sJoin.left());
  auto rightNode = toVeloxPlan(sJoin.right());

  auto outputRowType =
      leftNode->outputType()->unionWith(rightNode->outputType());

  auto joinType = join::fromProto(sJoin.type());

  if (joinType == core::JoinType::kLeftSemi) {
    outputRowType = leftNode->outputType();
  } else if (joinType == core::JoinType::kRightSemi) {
    outputRowType = rightNode->outputType();
  }

  // extract join keys from join expression
  std::vector<const ::substrait::Expression::FieldReference*> leftExprs,
      rightExprs;

  extractJoinKeys(sJoin.expression(), leftExprs, rightExprs);
  VELOX_CHECK_EQ(leftExprs.size(), rightExprs.size());

  std::vector<std::shared_ptr<const core::FieldAccessTypedExpr>> leftKeys,
      rightKeys;
  leftKeys.reserve(leftExprs.size());
  rightKeys.reserve(leftExprs.size());
  for (size_t i = 0; i < leftExprs.size(); ++i) {
    leftKeys.emplace_back(exprConverter_->toVeloxExpr(
        *leftExprs[i],
        leftNode->outputType()->unionWith(rightNode->outputType())));
    rightKeys.emplace_back(exprConverter_->toVeloxExpr(
        *rightExprs[i],
        leftNode->outputType()->unionWith(rightNode->outputType())));
  }

  core::TypedExprPtr filter;
  if (sJoin.has_post_join_filter()) {
    filter = exprConverter_->toVeloxExpr(
        sJoin.post_join_filter(),
        leftNode->outputType()->unionWith(rightNode->outputType()));
  }

  return std::make_shared<core::HashJoinNode>(
      nextPlanNodeId(),
      joinType,
      leftKeys,
      rightKeys,
      filter,
      leftNode,
      rightNode,
      outputRowType);
}

} // namespace facebook::velox::substrait
