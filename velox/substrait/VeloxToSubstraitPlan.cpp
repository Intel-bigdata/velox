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

#include "velox/substrait/VeloxToSubstraitPlan.h"
#include "velox/substrait/JoinUtils.h"

namespace facebook::velox::substrait {

namespace {
::substrait::AggregationPhase toAggregationPhase(
    core::AggregationNode::Step step) {
  switch (step) {
    case core::AggregationNode::Step::kPartial: {
      return ::substrait::AGGREGATION_PHASE_INITIAL_TO_INTERMEDIATE;
    }
    case core::AggregationNode::Step::kIntermediate: {
      return ::substrait::AGGREGATION_PHASE_INTERMEDIATE_TO_INTERMEDIATE;
    }
    case core::AggregationNode::Step::kSingle: {
      return ::substrait::AGGREGATION_PHASE_INITIAL_TO_RESULT;
    }
    case core::AggregationNode::Step::kFinal: {
      return ::substrait::AGGREGATION_PHASE_INTERMEDIATE_TO_RESULT;
    }
    default:
      VELOX_UNSUPPORTED(
          "Unsupported Aggregate Step '{}' in Substrait ",
          mapAggregationStepToName(step));
  }
}

::substrait::SortField_SortDirection toSortDirection(
    core::SortOrder sortOrder) {
  if (sortOrder.isNullsFirst() && sortOrder.isAscending()) {
    return ::substrait::SortField_SortDirection_SORT_DIRECTION_ASC_NULLS_FIRST;
  } else if (sortOrder.isNullsFirst() && !sortOrder.isAscending()) {
    return ::substrait::SortField_SortDirection_SORT_DIRECTION_DESC_NULLS_FIRST;
  } else if (!sortOrder.isNullsFirst() && sortOrder.isAscending()) {
    return ::substrait::SortField_SortDirection_SORT_DIRECTION_ASC_NULLS_LAST;
  } else if (!sortOrder.isNullsFirst() && !sortOrder.isAscending()) {
    return ::substrait::SortField_SortDirection_SORT_DIRECTION_DESC_NULLS_LAST;
  } else {
    VELOX_UNSUPPORTED(
        "SortOrder '{}' is not supported yet.", sortOrder.toString());
  }
}
} // namespace

::substrait::Plan& VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const core::PlanNodePtr& plan) {
  // Construct the extension colllector.
  extensionCollector_ = std::make_shared<SubstraitExtensionCollector>();
  // Construct the expression converter.
  exprConvertor_ =
      std::make_shared<VeloxToSubstraitExprConvertor>(extensionCollector_);

  auto substraitPlan =
      google::protobuf::Arena::CreateMessage<::substrait::Plan>(&arena);

  // Add unknown type in extension.
  auto unknownType = substraitPlan->add_extensions()->mutable_extension_type();

  unknownType->set_extension_uri_reference(0);
  unknownType->set_type_anchor(0);
  unknownType->set_name("UNKNOWN");

  // Do conversion.
  ::substrait::RelRoot* rootRel =
      substraitPlan->add_relations()->mutable_root();

  toSubstrait(arena, plan, rootRel->mutable_input());

  // Add extensions for all functions and types seen in the plan.
  extensionCollector_->addExtensionsToPlan(substraitPlan);

  // Set RootRel names.
  for (const auto& name : plan->outputType()->names()) {
    rootRel->add_names(name);
  }

  return *substraitPlan;
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const core::PlanNodePtr& planNode,
    ::substrait::Rel* rel) {
  if (auto filterNode =
          std::dynamic_pointer_cast<const core::FilterNode>(planNode)) {
    auto filterRel = rel->mutable_filter();
    toSubstrait(arena, filterNode, filterRel);
    return;
  }
  if (auto valuesNode =
          std::dynamic_pointer_cast<const core::ValuesNode>(planNode)) {
    ::substrait::ReadRel* readRel = rel->mutable_read();
    toSubstrait(arena, valuesNode, readRel);
    return;
  }
  if (auto projectNode =
          std::dynamic_pointer_cast<const core::ProjectNode>(planNode)) {
    ::substrait::ProjectRel* projectRel = rel->mutable_project();
    toSubstrait(arena, projectNode, projectRel);
    return;
  }
  if (auto aggregationNode =
          std::dynamic_pointer_cast<const core::AggregationNode>(planNode)) {
    ::substrait::AggregateRel* aggregateRel = rel->mutable_aggregate();
    toSubstrait(arena, aggregationNode, aggregateRel);
    return;
  }
  if (auto hashJoinNode =
          std::dynamic_pointer_cast<const core::HashJoinNode>(planNode)) {
    toSubstrait(arena, hashJoinNode, rel);
    return;
  }
  if (auto mergeJoinNode =
          std::dynamic_pointer_cast<const core::MergeJoinNode>(planNode)) {
    toSubstrait(arena, mergeJoinNode, rel);
    return;
  }
  if (auto crossJoinNode =
          std::dynamic_pointer_cast<const core::CrossJoinNode>(planNode)) {
    toSubstrait(arena, crossJoinNode, rel);
    return;
  }
  if (auto orderbyNode =
          std::dynamic_pointer_cast<const core::OrderByNode>(planNode)) {
    toSubstrait(arena, orderbyNode, rel->mutable_sort());
    return;
  }
  if (auto topNNode =
          std::dynamic_pointer_cast<const core::TopNNode>(planNode)) {
    // Convert it to fetchRel->sortRel.
    toSubstrait(arena, topNNode, rel->mutable_fetch());
    return;
  }
  if (auto limitNode =
          std::dynamic_pointer_cast<const core::LimitNode>(planNode)) {
    toSubstrait(arena, limitNode, rel->mutable_fetch());
    return;
  }
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::FilterNode>& filterNode,
    ::substrait::FilterRel* filterRel) {
  std::vector<core::PlanNodePtr> sources = filterNode->sources();

  // Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "Filter plan node must have exactly one source.");
  const auto& source = sources[0];

  ::substrait::Rel* filterInput = filterRel->mutable_input();
  // Build source.
  toSubstrait(arena, source, filterInput);

  // Construct substrait expr(Filter condition).
  auto filterCondition = filterNode->filter();
  auto inputType = source->outputType();
  filterRel->mutable_condition()->MergeFrom(
      exprConvertor_->toSubstraitExpr(arena, filterCondition, inputType));

  filterRel->mutable_common()->mutable_direct();
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::ValuesNode>& valuesNode,
    ::substrait::ReadRel* readRel) {
  const auto& outputType = valuesNode->outputType();

  ::substrait::ReadRel_VirtualTable* virtualTable =
      readRel->mutable_virtual_table();

  // The row number of the input data.
  int64_t numVectors = valuesNode->values().size();

  // There can be multiple rows in the data and each row is a RowVectorPtr.
  for (int64_t row = 0; row < numVectors; ++row) {
    // The row data.
    ::substrait::Expression_Literal_Struct* litValue =
        virtualTable->add_values();
    const auto& rowVector = valuesNode->values().at(row);
    // The column number of the row data.
    int64_t numColumns = rowVector->childrenSize();

    for (int64_t column = 0; column < numColumns; ++column) {
      ::substrait::Expression_Literal* substraitField =
          google::protobuf::Arena::CreateMessage<
              ::substrait::Expression_Literal>(&arena);

      const VectorPtr& child = rowVector->childAt(column);

      substraitField->MergeFrom(exprConvertor_->toSubstraitExpr(
          arena, std::make_shared<core::ConstantTypedExpr>(child), litValue));
    }
  }

  readRel->mutable_base_schema()->MergeFrom(
      typeConvertor_->toSubstraitNamedStruct(arena, outputType));

  readRel->mutable_common()->mutable_direct();
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::ProjectNode>& projectNode,
    ::substrait::ProjectRel* projectRel) {
  std::vector<std::shared_ptr<const core::ITypedExpr>> projections =
      projectNode->projections();

  std::vector<core::PlanNodePtr> sources = projectNode->sources();
  // Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "Project plan node must have exactly one source.");
  // The previous node.
  const auto& source = sources[0];

  // Process the source Node.
  ::substrait::Rel* projectRelInput = projectRel->mutable_input();
  toSubstrait(arena, source, projectRelInput);

  // Remap the output.
  ::substrait::RelCommon_Emit* projRelEmit =
      projectRel->mutable_common()->mutable_emit();

  int64_t projectionSize = projections.size();

  auto inputType = source->outputType();
  int64_t inputTypeSize = inputType->size();

  for (int64_t i = 0; i < projectionSize; i++) {
    const auto& veloxExpr = projections.at(i);

    projectRel->add_expressions()->MergeFrom(
        exprConvertor_->toSubstraitExpr(arena, veloxExpr, inputType));

    // Add outputMapping for each expression.
    projRelEmit->add_output_mapping(inputTypeSize + i);
  }
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::AggregationNode>& aggregateNode,
    ::substrait::AggregateRel* aggregateRel) {
  // Process the source Node.
  const auto& sources = aggregateNode->sources();
  // Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "Aggregation plan node must have exactly one source.");
  const auto& source = sources[0];

  // Build source.
  toSubstrait(arena, source, aggregateRel->mutable_input());

  // Convert aggregate grouping keys, such as: group by key1, key2.
  auto inputType = source->outputType();
  auto groupingKeys = aggregateNode->groupingKeys();
  int64_t groupingKeySize = groupingKeys.size();
  ::substrait::AggregateRel_Grouping* aggGroupings =
      aggregateRel->add_groupings();

  for (int64_t i = 0; i < groupingKeySize; i++) {
    aggGroupings->add_grouping_expressions()->MergeFrom(
        exprConvertor_->toSubstraitExpr(
            arena,
            std::dynamic_pointer_cast<const core::ITypedExpr>(
                groupingKeys.at(i)),
            inputType));
  }

  // AggregatesSize should be equal to or greater than the aggregateMasks Size.
  // Two cases: 1. aggregateMasksSize = 0, aggregatesSize > aggregateMasksSize.
  // 2. aggregateMasksSize != 0, aggregatesSize = aggregateMasksSize.
  auto aggregates = aggregateNode->aggregates();
  auto aggregateMasks = aggregateNode->aggregateMasks();
  int64_t aggregatesSize = aggregates.size();
  int64_t aggregateMasksSize = aggregateMasks.size();
  VELOX_CHECK_GE(aggregatesSize, aggregateMasksSize);

  for (int64_t i = 0; i < aggregatesSize; i++) {
    ::substrait::AggregateRel_Measure* aggMeasures =
        aggregateRel->add_measures();

    auto aggMaskExpr = aggregateMasks.at(i);
    // Set substrait filter.
    ::substrait::Expression* aggFilter = aggMeasures->mutable_filter();
    if (aggMaskExpr.get()) {
      aggFilter->MergeFrom(exprConvertor_->toSubstraitExpr(
          arena,
          std::dynamic_pointer_cast<const core::ITypedExpr>(aggMaskExpr),
          inputType));
    } else {
      // Set null.
      aggFilter = nullptr;
    }

    // Process measure, eg:sum(a).
    const auto& aggregatesExpr = aggregates.at(i);
    ::substrait::AggregateFunction* aggFunction =
        aggMeasures->mutable_measure();

    // Aggregation function name.
    const auto& funName = aggregatesExpr->name();
    // set aggFunction args.

    std::vector<TypePtr> arguments;
    arguments.reserve(aggregatesExpr->inputs().size());
    for (const auto& expr : aggregatesExpr->inputs()) {
      // If the expr is CallTypedExpr, people need to do project firstly.
      if (auto aggregatesExprInput =
              std::dynamic_pointer_cast<const core::CallTypedExpr>(expr)) {
        VELOX_NYI("In Velox Plan, the aggregates type cannot be CallTypedExpr");
      } else {
        aggFunction->add_arguments()->mutable_value()->MergeFrom(
            exprConvertor_->toSubstraitExpr(arena, expr, inputType));

        arguments.emplace_back(expr->type());
      }
    }

    auto referenceNumber = extensionCollector_->getReferenceNumber(
        funName, arguments, aggregateNode->step());

    aggFunction->set_function_reference(referenceNumber);

    aggFunction->mutable_output_type()->MergeFrom(
        typeConvertor_->toSubstraitType(arena, aggregatesExpr->type()));

    // Set substrait aggregate Function phase.
    aggFunction->set_phase(toAggregationPhase(aggregateNode->step()));
  }

  // Direct output.
  aggregateRel->mutable_common()->mutable_direct();
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::OrderByNode>& orderByNode,
    ::substrait::SortRel* sortRel) {
  std::vector<core::PlanNodePtr> sources = orderByNode->sources();

  // Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "OrderBy plan node must have exactly one source.");

  const auto& source = sources[0];

  // Build source.
  toSubstrait(arena, source, sortRel->mutable_input());

  // Process sortingKeys and sortingOrders.
  sortRel->MergeFrom(processSortFields(
      arena,
      orderByNode->sortingKeys(),
      orderByNode->sortingOrders(),
      source->outputType()));

  sortRel->set_is_partial(orderByNode->isPartial());
  sortRel->mutable_common()->mutable_direct();
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::TopNNode>& topNNode,
    ::substrait::FetchRel* fetchRel) {
  std::vector<core::PlanNodePtr> sources = topNNode->sources();
  // Convert it to be fetchRel->SortRel.
  //  Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "Top-N plan node must have exactly one source.");
  const auto& source = sources[0];

  // Construct the sortRel as the FetchRel input.
  ::substrait::SortRel* sortRel = fetchRel->mutable_input()->mutable_sort();

  // Build source.
  toSubstrait(arena, source, sortRel->mutable_input());

  sortRel->MergeFrom(processSortFields(
      arena,
      topNNode->sortingKeys(),
      topNNode->sortingOrders(),
      source->outputType()));

  sortRel->set_is_partial(topNNode->isPartial());
  sortRel->mutable_common()->mutable_direct();

  fetchRel->set_is_partial(topNNode->isPartial());
  fetchRel->set_count(topNNode->count());
  fetchRel->mutable_common()->mutable_direct();
}

const ::substrait::SortRel& VeloxToSubstraitPlanConvertor::processSortFields(
    google::protobuf::Arena& arena,
    const std::vector<core::FieldAccessTypedExprPtr>& sortingKeys,
    const std::vector<core::SortOrder>& sortingOrders,
    const facebook::velox::RowTypePtr& inputType) {
  ::substrait::SortRel* sortRel =
      google::protobuf::Arena::CreateMessage<::substrait::SortRel>(&arena);

  VELOX_CHECK_EQ(
      sortingKeys.size(),
      sortingOrders.size(),
      "Number of sorting keys and sorting orders must be the same");

  for (int64_t i = 0; i < sortingKeys.size(); i++) {
    ::substrait::SortField* sortField = sortRel->add_sorts();
    sortField->mutable_expr()->MergeFrom(exprConvertor_->toSubstraitExpr(
        arena,
        std::dynamic_pointer_cast<const core::ITypedExpr>(sortingKeys[i]),
        inputType));

    sortField->set_direction(toSortDirection(sortingOrders[i]));
  }
  return *sortRel;
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::LimitNode>& limitNode,
    ::substrait::FetchRel* fetchRel) {
  std::vector<core::PlanNodePtr> sources = limitNode->sources();

  // Check there only have one input.
  VELOX_USER_CHECK_EQ(
      1, sources.size(), "Limit plan node must have exactly one source.");

  // Build source.
  toSubstrait(arena, sources[0], fetchRel->mutable_input());

  fetchRel->set_offset(limitNode->offset());
  fetchRel->set_count(limitNode->count());
  fetchRel->set_is_partial(limitNode->isPartial());
  fetchRel->mutable_common()->mutable_direct();
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::HashJoinNode> hashJoinNode,
    ::substrait::Rel* rel) {
  auto [joinLeftSourceOutput, joinRightSourceOutput] =
      getTwoSourcesOutput(hashJoinNode);

  auto joinOutputRowType =
      joinLeftSourceOutput->unionWith(joinRightSourceOutput);

  if (hashJoinNode->joinType() == core::JoinType::kLeftSemi) {
    joinOutputRowType = joinLeftSourceOutput;
  } else if (hashJoinNode->joinType() == core::JoinType::kRightSemi) {
    joinOutputRowType = joinRightSourceOutput;
  }

  // Insert a project rel for outputRowType of HashJoinNode.
  auto projectRel = rel->mutable_project();
  constructProjectRel(arena, projectRel, hashJoinNode, joinOutputRowType);

  auto hashJoinRel = projectRel->mutable_input()->mutable_hash_join();

  // Convert the input node.
  toSubstrait(arena, hashJoinNode->sources()[0], hashJoinRel->mutable_left());
  toSubstrait(arena, hashJoinNode->sources()[1], hashJoinRel->mutable_right());

  // Convert the left and right keys.
  VELOX_CHECK_EQ(
      hashJoinNode->leftKeys().size(), hashJoinNode->rightKeys().size());

  auto substraitLeftKeys = hashJoinRel->add_left_keys();
  auto substraitRightKeys = hashJoinRel->add_right_keys();
  for (auto leftKey : hashJoinNode->leftKeys()) {
    substraitLeftKeys->mutable_expression()->mutable_selection()->MergeFrom(
        exprConvertor_->toSubstraitExpr(arena, leftKey, joinLeftSourceOutput));
  }
  for (auto rightKey : hashJoinNode->rightKeys()) {
    substraitRightKeys->mutable_expression()->mutable_selection()->MergeFrom(
        exprConvertor_->toSubstraitExpr(
            arena, rightKey, joinRightSourceOutput));
  }

  if (hashJoinNode->filter()) {
    // Set the join filter.
    hashJoinRel->mutable_post_join_filter()->MergeFrom(
        exprConvertor_->toSubstraitExpr(
            arena,
            hashJoinNode->filter(),
            joinLeftSourceOutput->unionWith(joinRightSourceOutput)));
  }
  hashJoinRel->mutable_common()->mutable_direct();

  hashJoinRel->set_type(join::toHashProto(hashJoinNode->joinType()));
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::MergeJoinNode> mergeJoinNode,
    ::substrait::Rel* rel) {
  auto [joinLeftSourceOutput, joinRightSourceOutput] =
      getTwoSourcesOutput(mergeJoinNode);

  auto joinOutputRowType =
      joinLeftSourceOutput->unionWith(joinRightSourceOutput);

  VELOX_USER_CHECK(
      mergeJoinNode->isInnerJoin() || mergeJoinNode->isLeftJoin(),
      "Merge join supports only inner and left joins. Other join types are not supported yet.");

  // Insert a project rel for outputRowType of MergeJoinNode.
  auto projectRel = rel->mutable_project();
  constructProjectRel(arena, projectRel, mergeJoinNode, joinOutputRowType);

  auto mergeJoinRel = projectRel->mutable_input()->mutable_merge_join();

  // Convert the input node.
  toSubstrait(arena, mergeJoinNode->sources()[0], mergeJoinRel->mutable_left());
  toSubstrait(
      arena, mergeJoinNode->sources()[1], mergeJoinRel->mutable_right());

  // Convert the left and right keys.
  VELOX_CHECK_EQ(
      mergeJoinNode->leftKeys().size(), mergeJoinNode->rightKeys().size());

  auto substraitLeftKeys = mergeJoinRel->add_left_keys();
  auto substraitRightKeys = mergeJoinRel->add_right_keys();
  for (auto leftKey : mergeJoinNode->leftKeys()) {
    substraitLeftKeys->mutable_expression()->mutable_selection()->MergeFrom(
        exprConvertor_->toSubstraitExpr(arena, leftKey, joinLeftSourceOutput));
  }
  for (auto rightKey : mergeJoinNode->rightKeys()) {
    substraitRightKeys->mutable_expression()->mutable_selection()->MergeFrom(
        exprConvertor_->toSubstraitExpr(
            arena, rightKey, joinRightSourceOutput));
  }

  if (mergeJoinNode->filter()) {
    // Set the join filter.
    mergeJoinRel->mutable_post_join_filter()->MergeFrom(
        exprConvertor_->toSubstraitExpr(
            arena,
            mergeJoinNode->filter(),
            joinLeftSourceOutput->unionWith(joinRightSourceOutput)));
  }
  mergeJoinRel->mutable_common()->mutable_direct();

  mergeJoinRel->set_type(join::toMergeProto(mergeJoinNode->joinType()));
}

void VeloxToSubstraitPlanConvertor::toSubstrait(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::CrossJoinNode> crossJoinNode,
    ::substrait::Rel* crossJoinRel) {
  auto [joinLeftSourceOutput, joinRightSourceOutput] =
      getTwoSourcesOutput(crossJoinNode);

  auto joinOutputRowType =
      joinLeftSourceOutput->unionWith(joinRightSourceOutput);

  // Insert a project rel for outputRowType of HashJoinNode.
  auto projectRel = crossJoinRel->mutable_project();
  constructProjectRel(arena, projectRel, crossJoinNode, joinOutputRowType);

  auto crossRel = projectRel->mutable_input()->mutable_cross();

  // Convert the input node.
  toSubstrait(arena, crossJoinNode->sources()[0], crossRel->mutable_left());
  toSubstrait(arena, crossJoinNode->sources()[1], crossRel->mutable_right());

  crossRel->mutable_common()->mutable_direct();
}

const std::pair<const RowTypePtr&, const RowTypePtr&>
VeloxToSubstraitPlanConvertor::getTwoSourcesOutput(
    const std::shared_ptr<const core::PlanNode>& node) {
  // JoinNode has exactly two input nodes.
  VELOX_USER_CHECK_EQ(
      2,
      node->sources().size(),
      "Join plan node must have exactly two sources.");

  const auto& joinLeftSourceOutput = node->sources()[0]->outputType();
  const auto& joinRightSourceOutput = node->sources()[1]->outputType();

  return {joinLeftSourceOutput, joinRightSourceOutput};
}

void VeloxToSubstraitPlanConvertor::constructProjectRel(
    google::protobuf::Arena& arena,
    ::substrait::ProjectRel* projectRel,
    const std::shared_ptr<const core::PlanNode>& joinNode,
    const facebook::velox::RowTypePtr& joinOutputRowType) {
  auto projectEmitRel = projectRel->mutable_common()->mutable_emit();
  auto joinOutputTypeSize = joinOutputRowType->size();
  for (auto i = 0; i < joinNode->outputType()->size(); i++) {
    auto fieldRef = std::make_shared<const core::FieldAccessTypedExpr>(
        joinNode->outputType()->childAt(i), joinNode->outputType()->nameOf(i));
    projectRel->add_expressions()->mutable_selection()->MergeFrom(
        exprConvertor_->toSubstraitExpr(arena, fieldRef, joinOutputRowType));
    projectEmitRel->add_output_mapping(joinOutputTypeSize + i);
  }
}

} // namespace facebook::velox::substrait
