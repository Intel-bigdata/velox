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

#include "velox/substrait/VeloxToSubstraitExpr.h"
#include "velox/vector/FlatVector.h"

namespace facebook::velox::substrait {

namespace {
std::shared_ptr<VeloxToSubstraitExprConvertor> exprConvertor_;

template <TypeKind sourceKind>
void convertVectorValue(
    google::protobuf::Arena& arena,
    const velox::VectorPtr& vectorValue,
    ::substrait::Expression_Literal_Struct* litValue,
    ::substrait::Expression_Literal* substraitField) {
  const TypePtr& childType = vectorValue->type();

  using T = typename TypeTraits<sourceKind>::NativeType;

  auto childToFlatVec = vectorValue->asFlatVector<T>();

  //  Get the batchSize and convert each value in it.
  vector_size_t flatVecSize = childToFlatVec->size();
  for (int64_t i = 0; i < flatVecSize; i++) {
    substraitField = litValue->add_fields();
    if (childToFlatVec->isNullAt(i)) {
      // Process the null value.
      substraitField->MergeFrom(
          exprConvertor_->toSubstraitNullLiteral(arena, childType->kind()));
    } else {
      substraitField->MergeFrom(exprConvertor_->toSubstraitNotNullLiteral(
          arena, static_cast<const variant>(childToFlatVec->valueAt(i))));
    }
  }
}

} // namespace

const ::substrait::Expression& VeloxToSubstraitExprConvertor::toSubstraitExpr(
    google::protobuf::Arena& arena,
    const core::TypedExprPtr& expr,
    const RowTypePtr& inputType) {
  ::substrait::Expression* substraitExpr =
      google::protobuf::Arena::CreateMessage<::substrait::Expression>(&arena);
  if (auto constExpr =
          std::dynamic_pointer_cast<const core::ConstantTypedExpr>(expr)) {
    substraitExpr->mutable_literal()->MergeFrom(
        toSubstraitExpr(arena, constExpr));
    return *substraitExpr;
  }
  if (auto callTypeExpr =
          std::dynamic_pointer_cast<const core::CallTypedExpr>(expr)) {
    substraitExpr->MergeFrom(toSubstraitExpr(arena, callTypeExpr, inputType));
    return *substraitExpr;
  }
  if (auto fieldExpr =
          std::dynamic_pointer_cast<const core::FieldAccessTypedExpr>(expr)) {
    substraitExpr->mutable_selection()->MergeFrom(
        toSubstraitExpr(arena, fieldExpr, inputType));

    return *substraitExpr;
  }
  if (auto castExpr =
          std::dynamic_pointer_cast<const core::CastTypedExpr>(expr)) {
    substraitExpr->mutable_cast()->MergeFrom(
        toSubstraitExpr(arena, castExpr, inputType));

    return *substraitExpr;
  }

  VELOX_UNSUPPORTED("Unsupport Expr '{}' in Substrait", expr->toString());
}

const ::substrait::Expression_Cast&
VeloxToSubstraitExprConvertor::toSubstraitExpr(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::CastTypedExpr>& castExpr,
    const RowTypePtr& inputType) {
  ::substrait::Expression_Cast* substraitCastExpr =
      google::protobuf::Arena::CreateMessage<::substrait::Expression_Cast>(
          &arena);
  std::vector<core::TypedExprPtr> castExprInputs = castExpr->inputs();

  substraitCastExpr->mutable_type()->MergeFrom(
      typeConvertor_->toSubstraitType(arena, castExpr->type()));

  for (auto& arg : castExprInputs) {
    substraitCastExpr->mutable_input()->MergeFrom(
        toSubstraitExpr(arena, arg, inputType));
  }
  return *substraitCastExpr;
}

const ::substrait::Expression_FieldReference&
VeloxToSubstraitExprConvertor::toSubstraitExpr(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::FieldAccessTypedExpr>& fieldExpr,
    const RowTypePtr& inputType) {
  ::substrait::Expression_FieldReference* substraitFieldExpr =
      google::protobuf::Arena::CreateMessage<
          ::substrait::Expression_FieldReference>(&arena);

  std::string exprName = fieldExpr->name();

  ::substrait::Expression_ReferenceSegment_StructField* directStruct =
      substraitFieldExpr->mutable_direct_reference()->mutable_struct_field();
  // Add root_reference for direct_reference with struct_field.
  substraitFieldExpr->mutable_root_reference();
  directStruct->set_field(inputType->getChildIdx(exprName));
  return *substraitFieldExpr;
}

const ::substrait::Expression& VeloxToSubstraitExprConvertor::toSubstraitExpr(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::CallTypedExpr>& callTypeExpr,
    const RowTypePtr& inputType) {
  ::substrait::Expression* substraitExpr =
      google::protobuf::Arena::CreateMessage<::substrait::Expression>(&arena);

  auto inputs = callTypeExpr->inputs();
  auto& functionName = callTypeExpr->name();

  if (functionName != "if" && functionName != "switch") {
    ::substrait::Expression_ScalarFunction* scalarExpr =
        substraitExpr->mutable_scalar_function();

    std::vector<TypePtr> types;
    types.reserve(callTypeExpr->inputs().size());
    for (auto& typedExpr : callTypeExpr->inputs()) {
      types.emplace_back(typedExpr->type());
    }

    scalarExpr->set_function_reference(
        extensionCollector_->getReferenceNumber(functionName, types));

    for (auto& arg : inputs) {
      scalarExpr->add_arguments()->mutable_value()->MergeFrom(
          toSubstraitExpr(arena, arg, inputType));
    }

    scalarExpr->mutable_output_type()->MergeFrom(
        typeConvertor_->toSubstraitType(arena, callTypeExpr->type()));

  } else {
    // For today's version of Substrait, you'd have to use IfThen if you need
    // the switch cases to be call typed expression.

    size_t inputsSize = callTypeExpr->inputs().size();
    bool hasElseInput = inputsSize % 2 == 1;

    auto ifThenExpr = substraitExpr->mutable_if_then();
    for (int i = 0; i < inputsSize / 2; i++) {
      auto ifClauseExpr = ifThenExpr->add_ifs();
      ifClauseExpr->mutable_if_()->MergeFrom(
          toSubstraitExpr(arena, callTypeExpr->inputs().at(i * 2), inputType));
      ifClauseExpr->mutable_then()->MergeFrom(toSubstraitExpr(
          arena, callTypeExpr->inputs().at(i * 2 + 1), inputType));
    }
    if (hasElseInput) {
      ifThenExpr->mutable_else_()->MergeFrom(toSubstraitExpr(
          arena, callTypeExpr->inputs().at(inputsSize - 1), inputType));
    }
  }

  return *substraitExpr;
}

const ::substrait::Expression_Literal&
VeloxToSubstraitExprConvertor::toSubstraitExpr(
    google::protobuf::Arena& arena,
    const std::shared_ptr<const core::ConstantTypedExpr>& constExpr,
    ::substrait::Expression_Literal_Struct* litValue) {
  if (constExpr->hasValueVector()) {
    return toSubstraitLiteral(arena, constExpr->valueVector(), litValue);
  } else {
    return toSubstraitLiteral(arena, constExpr->value());
  }
}

const ::substrait::Expression_Literal&
VeloxToSubstraitExprConvertor::toSubstraitLiteral(
    google::protobuf::Arena& arena,
    const velox::variant& variantValue) {
  ::substrait::Expression_Literal* literalExpr =
      google::protobuf::Arena::CreateMessage<::substrait::Expression_Literal>(
          &arena);

  if (variantValue.isNull()) {
    literalExpr->MergeFrom(toSubstraitNullLiteral(arena, variantValue.kind()));
  } else {
    literalExpr->MergeFrom(toSubstraitNotNullLiteral(arena, variantValue));
  }
  return *literalExpr;
}

template <TypeKind flatKind>
void VeloxToSubstraitExprConvertor::flatVectorToListLiteral(
    google::protobuf::Arena& arena,
    const velox::VectorPtr& vector,
    ::substrait::Expression_Literal_List* listLiteral) {
  using T = typename TypeTraits<flatKind>::NativeType;
  if (auto flatVector = std::dynamic_pointer_cast<FlatVector<T>>(vector)) {
    const TypePtr& childType = flatVector->type();
    for (int idx = 0; idx < flatVector->size(); idx++) {
      ::substrait::Expression_Literal* childLiteral = listLiteral->add_values();
      if (flatVector->isNullAt(idx)) {
        // Process the null value.
        childLiteral->MergeFrom(
            exprConvertor_->toSubstraitNullLiteral(arena, childType->kind()));
      } else {
        childLiteral->MergeFrom(exprConvertor_->toSubstraitNotNullLiteral(
            arena, static_cast<const variant>(flatVector->valueAt(idx))));
      }
    }
  } else {
    VELOX_FAIL("Flat vector is expected.");
  }
}

void VeloxToSubstraitExprConvertor::complexVectorToLiteral(
    google::protobuf::Arena& arena,
    std::shared_ptr<ConstantVector<ComplexType>> constantVector,
    ::substrait::Expression_Literal* substraitLiteral) {
  VELOX_CHECK_EQ(
      constantVector->size(), 1, "Only one complex vector is expected.");
  if (auto arrayVector = std::dynamic_pointer_cast<ArrayVector>(
          constantVector->valueVector())) {
    VELOX_CHECK_EQ(arrayVector->size(), 1, "Only one array is expected.");
    if (constantVector->isNullAt(0)) {
      // Process the null value.
      substraitLiteral->MergeFrom(exprConvertor_->toSubstraitNullLiteral(
          arena, arrayVector->type()->kind()));
    } else {
      ::substrait::Expression_Literal_List* listLiteral =
          google::protobuf::Arena::CreateMessage<
              ::substrait::Expression_Literal_List>(&arena);
      if (arrayVector->elements()->isScalar()) {
        VELOX_DYNAMIC_SCALAR_TYPE_DISPATCH(
            flatVectorToListLiteral,
            arrayVector->elements()->type()->kind(),
            arena,
            arrayVector->elements(),
            listLiteral);
        substraitLiteral->mutable_list()->MergeFrom(*listLiteral);
      } else {
        VELOX_NYI(
            "To Substrait literal is not supported for {}.",
            arrayVector->elements()->type()->toString());
      }
    }
  } else {
    VELOX_NYI(
        "To Substrait literal is not supported for {}.",
        constantVector->type()->toString());
  }
}

const ::substrait::Expression_Literal&
VeloxToSubstraitExprConvertor::toSubstraitLiteral(
    google::protobuf::Arena& arena,
    const velox::VectorPtr& vectorValue,
    ::substrait::Expression_Literal_Struct* litValue) {
  ::substrait::Expression_Literal* substraitField =
      google::protobuf::Arena::CreateMessage<::substrait::Expression_Literal>(
          &arena);

  if (vectorValue->isScalar()) {
    VELOX_DYNAMIC_SCALAR_TYPE_DISPATCH(
        convertVectorValue,
        vectorValue->type()->kind(),
        arena,
        vectorValue,
        litValue,
        substraitField);
  } else if (
      auto constantVector =
          std::dynamic_pointer_cast<ConstantVector<ComplexType>>(vectorValue)) {
    complexVectorToLiteral(arena, constantVector, substraitField);
  } else {
    VELOX_NYI(
        "To Substrait literal is not supported for {}.",
        vectorValue->type()->toString());
  }

  return *substraitField;
}

const ::substrait::Expression_Literal&
VeloxToSubstraitExprConvertor::toSubstraitNotNullLiteral(
    google::protobuf::Arena& arena,
    const velox::variant& variantValue) {
  ::substrait::Expression_Literal* literalExpr =
      google::protobuf::Arena::CreateMessage<::substrait::Expression_Literal>(
          &arena);
  switch (variantValue.kind()) {
    case velox::TypeKind::BOOLEAN: {
      literalExpr->set_boolean(variantValue.value<TypeKind::BOOLEAN>());
      break;
    }
    case velox::TypeKind::TINYINT: {
      literalExpr->set_i8(variantValue.value<TypeKind::TINYINT>());
      break;
    }
    case velox::TypeKind::SMALLINT: {
      literalExpr->set_i16(variantValue.value<TypeKind::SMALLINT>());
      break;
    }
    case velox::TypeKind::INTEGER: {
      literalExpr->set_i32(variantValue.value<TypeKind::INTEGER>());
      break;
    }
    case velox::TypeKind::BIGINT: {
      literalExpr->set_i64(variantValue.value<TypeKind::BIGINT>());
      break;
    }
    case velox::TypeKind::REAL: {
      literalExpr->set_fp32(variantValue.value<TypeKind::REAL>());
      break;
    }
    case velox::TypeKind::DOUBLE: {
      literalExpr->set_fp64(variantValue.value<TypeKind::DOUBLE>());
      break;
    }
    case velox::TypeKind::VARCHAR: {
      auto vCharValue = variantValue.value<StringView>();
      ::substrait::Expression_Literal::VarChar* sVarChar =
          new ::substrait::Expression_Literal::VarChar();
      sVarChar->set_value(vCharValue.data());
      sVarChar->set_length(vCharValue.size());
      literalExpr->set_allocated_var_char(sVarChar);
      break;
    }
    case velox::TypeKind::TIMESTAMP: {
      literalExpr->set_timestamp(
          variantValue.value<TypeKind::TIMESTAMP>().getNanos());
      break;
    }
    default:
      VELOX_NYI(
          "Unsupported constant Type '{}' ",
          mapTypeKindToName(variantValue.kind()));
  }

  literalExpr->set_nullable(false);

  return *literalExpr;
}

const ::substrait::Expression_Literal&
VeloxToSubstraitExprConvertor::toSubstraitNullLiteral(
    google::protobuf::Arena& arena,
    const velox::TypeKind& typeKind) {
  ::substrait::Expression_Literal* substraitField =
      google::protobuf::Arena::CreateMessage<::substrait::Expression_Literal>(
          &arena);
  switch (typeKind) {
    case velox::TypeKind::BOOLEAN: {
      ::substrait::Type_Boolean* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_Boolean>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_bool_(nullValue);
      break;
    }
    case velox::TypeKind::TINYINT: {
      ::substrait::Type_I8* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_I8>(&arena);

      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_i8(nullValue);
      break;
    }
    case velox::TypeKind::SMALLINT: {
      ::substrait::Type_I16* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_I16>(&arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_i16(nullValue);
      break;
    }
    case velox::TypeKind::INTEGER: {
      ::substrait::Type_I32* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_I32>(&arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_i32(nullValue);
      break;
    }
    case velox::TypeKind::BIGINT: {
      ::substrait::Type_I64* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_I64>(&arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_i64(nullValue);
      break;
    }
    case velox::TypeKind::VARCHAR: {
      ::substrait::Type_String* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_String>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_string(nullValue);
      break;
    }
    case velox::TypeKind::REAL: {
      ::substrait::Type_FP32* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_FP32>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_fp32(nullValue);
      break;
    }
    case velox::TypeKind::DOUBLE: {
      ::substrait::Type_FP64* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_FP64>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_fp64(nullValue);
      break;
    }
    case velox::TypeKind::ARRAY: {
      ::substrait::Type_List* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_List>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      substraitField->mutable_null()->set_allocated_list(nullValue);
      break;
    }
    case velox::TypeKind::UNKNOWN: {
      ::substrait::Type_UserDefined* nullValue =
          google::protobuf::Arena::CreateMessage<::substrait::Type_UserDefined>(
              &arena);
      nullValue->set_nullability(
          ::substrait::Type_Nullability_NULLABILITY_NULLABLE);
      nullValue->set_type_reference(0);
      substraitField->mutable_null()->set_allocated_user_defined(nullValue);

      break;
    }
    default: {
      VELOX_UNSUPPORTED("Unsupported type '{}'", mapTypeKindToName(typeKind));
    }
  }
  substraitField->set_nullable(true);
  return *substraitField;
}

} // namespace facebook::velox::substrait
