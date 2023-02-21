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

#include "velox/substrait/JoinUtils.h"

namespace facebook::velox::substrait {

namespace join {

::substrait::JoinRel_JoinType toProto(core::JoinType joinType) {
  switch (joinType) {
    case core::JoinType::kInner:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_INNER;
    case core::JoinType::kLeft:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_LEFT;
    case core::JoinType::kRight:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_RIGHT;
    case core::JoinType::kFull:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_OUTER;
    case core::JoinType::kLeftSemi:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_SEMI;
    case core::JoinType::kNullAwareAnti:
      return ::substrait::JoinRel_JoinType_JOIN_TYPE_ANTI;
    default:
      VELOX_UNSUPPORTED(
          "toProto not supported for velox join type, {}", joinType);
  }
}

::substrait::HashJoinRel_JoinType toHashProto(core::JoinType joinType){
  switch(joinType){
    case core::JoinType::kInner:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_INNER;
    case core::JoinType::kFull:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_OUTER;
    case core::JoinType::kLeft:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_LEFT;
    case core::JoinType::kRight:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_RIGHT;
    case core::JoinType::kLeftSemi:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_LEFT_SEMI;
    case core::JoinType::kRightSemi:
      return ::substrait::HashJoinRel_JoinType_JOIN_TYPE_RIGHT_SEMI;
    default:
      VELOX_UNSUPPORTED(
          "toHashProto not supported for velox join type, {}", joinType);

  }
}

::substrait::MergeJoinRel_JoinType toMergeProto(core::JoinType joinType){
  switch(joinType){
    case core::JoinType::kInner:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_INNER;
    case core::JoinType::kFull:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_OUTER;
    case core::JoinType::kLeft:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_LEFT;
    case core::JoinType::kRight:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_RIGHT;
    case core::JoinType::kLeftSemi:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_LEFT_SEMI;
    case core::JoinType::kRightSemi:
      return ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_RIGHT_SEMI;
    default:
      VELOX_UNSUPPORTED(
          "toMergeProto not supported for velox join type, {}", joinType);

  }
}

core::JoinType fromProto(::substrait::JoinRel_JoinType joinType) {
  switch (joinType) {
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_INNER:
      return core::JoinType::kInner;
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_LEFT:
      return core::JoinType::kLeft;
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_RIGHT:
      return core::JoinType::kRight;
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_OUTER:
      return core::JoinType::kFull;
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_SEMI:
      return core::JoinType::kLeftSemi;
    case ::substrait::JoinRel_JoinType_JOIN_TYPE_ANTI:
      return core::JoinType::kNullAwareAnti;
    default:
      VELOX_UNSUPPORTED("Unsupported substrait join type, {}", joinType);
  }
}

core::JoinType fromProto(::substrait::HashJoinRel_JoinType hashJoinType){
  switch (hashJoinType) {
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_INNER:
      return core::JoinType::kInner;
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_OUTER:
      return core::JoinType::kFull;
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_LEFT:
      return core::JoinType::kLeft;
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_RIGHT:
      return core::JoinType::kRight;
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_LEFT_SEMI:
      return core::JoinType::kLeftSemi;
    case ::substrait::HashJoinRel_JoinType_JOIN_TYPE_RIGHT_SEMI:
      return core::JoinType::kRightSemi;
    default:
      VELOX_UNSUPPORTED("Unsupported substrait join type, {}", hashJoinType);
  }
}

core::JoinType fromProto(::substrait::MergeJoinRel_JoinType mergeJoinType){
  switch (mergeJoinType) {
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_INNER:
      return core::JoinType::kInner;
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_OUTER:
      return core::JoinType::kFull;
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_LEFT:
      return core::JoinType::kLeft;
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_RIGHT:
      return core::JoinType::kRight;
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_LEFT_SEMI:
      return core::JoinType::kLeftSemi;
    case ::substrait::MergeJoinRel_JoinType_JOIN_TYPE_RIGHT_SEMI:
      return core::JoinType::kRightSemi;
    default:
      VELOX_UNSUPPORTED("Unsupported substrait join type, {}", mergeJoinType);
  }
}

} // namespace join

} // namespace facebook::velox::substrait