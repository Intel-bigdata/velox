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

#pragma once

#include <iostream>
#include "velox/core/ITypedExpr.h"
#include "velox/substrait/proto/substrait/algebra.pb.h"

namespace facebook::velox::substrait {

using SubstraitTypeKind = ::substrait::Type::KindCase;

template <SubstraitTypeKind KIND>
struct SubstraitTypeTraits {};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kBool> {
  static constexpr const char* signature = "bool";
  static constexpr const char* typeString = "boolean";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI8> {
  static constexpr const char* signature = "i8";
  static constexpr const char* typeString = "i8";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI16> {
  static constexpr const char* signature = "i16";
  static constexpr const char* typeString = "i16";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI32> {
  static constexpr const char* signature = "i32";
  static constexpr const char* typeString = "i32";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI64> {
  static constexpr const char* signature = "i64";
  static constexpr const char* typeString = "i64";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFp32> {
  static constexpr const char* signature = "fp32";
  static constexpr const char* typeString = "fp32";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFp64> {
  static constexpr const char* signature = "fp64";
  static constexpr const char* typeString = "fp64";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kString> {
  static constexpr const char* signature = "str";
  static constexpr const char* typeString = "string";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kBinary> {
  static constexpr const char* signature = "vbin";
  static constexpr const char* typeString = "binary";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTimestamp> {
  static constexpr const char* signature = "ts";
  static constexpr const char* typeString = "timestamp";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTimestampTz> {
  static constexpr const char* signature = "tstz";
  static constexpr const char* typeString = "timestamp_tz";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kDate> {
  static constexpr const char* signature = "date";
  static constexpr const char* typeString = "date";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTime> {
  static constexpr const char* signature = "time";
  static constexpr const char* typeString = "time";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kIntervalYear> {
  static constexpr const char* signature = "iyear";
  static constexpr const char* typeString = "interval_year";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kIntervalDay> {
  static constexpr const char* signature = "iday";
  static constexpr const char* typeString = "interval_day";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kUuid> {
  static constexpr const char* signature = "uuid";
  static constexpr const char* typeString = "uuid";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFixedChar> {
  static constexpr const char* signature = "fchar";
  static constexpr const char* typeString = "fixedchar";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kVarchar> {
  static constexpr const char* signature = "vchar";
  static constexpr const char* typeString = "varchar";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFixedBinary> {
  static constexpr const char* signature = "fbin";
  static constexpr const char* typeString = "fixedbinary";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kDecimal> {
  static constexpr const char* signature = "dec";
  static constexpr const char* typeString = "decimal";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kStruct> {
  static constexpr const char* signature = "struct";
  static constexpr const char* typeString = "struct";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kList> {
  static constexpr const char* signature = "list";
  static constexpr const char* typeString = "list";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kMap> {
  static constexpr const char* signature = "map";
  static constexpr const char* typeString = "map";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kUserDefined> {
  static constexpr const char* signature = "u!name";
  static constexpr const char* typeString = "user defined type";
};

template <TypeKind T>
class SubstraitTypeCreator;

class SubstraitType {
 public:
  /// deserialize substrait raw type string into Substrait extension  type.
  /// @param rawType - substrait extension raw string type
  static std::shared_ptr<const SubstraitType> decode(
      const std::string& rawType);

  /// Return the Substrait extension type  according to the velox type.
  static std::shared_ptr<const SubstraitType> fromVelox(const TypePtr& type) {
    if (type) {
      return VELOX_DYNAMIC_TYPE_DISPATCH(
          substraitTypeMaker, type->kind(), type);
    }
    return nullptr;
  }

  /// template class for create SubstraitType by given velox TypeKind
  template <TypeKind T>
  static std::shared_ptr<const SubstraitType> substraitTypeMaker(
      const TypePtr& type) {
    return SubstraitTypeCreator<T>::create(type);
  }

  /// signature name of substrait type.
  virtual const std::string signature() const = 0;

  /// test type is a Wildcard type or not.
  virtual const bool isWildcard() const {
    return false;
  }

  /// unknown type,see @SubstraitUnknownType
  virtual const bool isUnknown() const {
    return false;
  }

  /// a known substrait type kind
  virtual const SubstraitTypeKind kind() const = 0;

  virtual const std::string typeString() const = 0;

  /// whether two types are same as each other
  virtual bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const {
    return kind() == other->kind();
  }

 private:
  /// A map store the raw type string and corresponding Substrait Type
  static const std::
      unordered_map<std::string, std::shared_ptr<const SubstraitType>>&
      scalarTypeMapping();
};

using SubstraitTypePtr = std::shared_ptr<const SubstraitType>;

using SubstraitTypePtr = std::shared_ptr<const SubstraitType>;

/// Types used in function argument declarations.
template <SubstraitTypeKind Kind>
class SubstraitTypeBase : public SubstraitType {
 public:
  const std::string signature() const override {
    return SubstraitTypeTraits<Kind>::signature;
  }

  virtual const SubstraitTypeKind kind() const override {
    return Kind;
  }

  const std::string typeString() const override {
    return SubstraitTypeTraits<Kind>::typeString;
  }
};

/// A string literal type can present the 'any1'
class SubstraitStringLiteralType : public SubstraitType {
 public:
  SubstraitStringLiteralType(const std::string& value) : value_(value) {}

  const std::string& value() const {
    return value_;
  }

  const std::string signature() const override {
    return value_;
  }

  const std::string typeString() const override {
    return value_;
  }
  const bool isWildcard() const override {
    return value_.find("any") == 0;
  }

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
                other)) {
      return type->value_ == value_;
    }
    return false;
  }

  const SubstraitTypeKind kind() const override {
    return SubstraitTypeKind ::KIND_NOT_SET;
  }

 private:
  /// raw string of wildcard type.
  const std::string value_;
};

using SubstraitStringLiteralTypePtr =
    std::shared_ptr<const SubstraitStringLiteralType>;

class SubstraitDecimalType
    : public SubstraitTypeBase<SubstraitTypeKind ::kDecimal> {
 public:
  SubstraitDecimalType(
      const SubstraitStringLiteralTypePtr& precision,
      const SubstraitStringLiteralTypePtr& scale)
      : precision_(precision), scale_(scale) {}

  SubstraitDecimalType(const std::string& precision, const std::string& scale)
      : precision_(std::make_shared<SubstraitStringLiteralType>(precision)),
        scale_(std::make_shared<SubstraitStringLiteralType>(scale)) {}

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitDecimalType>(other)) {
      return type->scale_ == scale_ && type->precision_ == precision_;
    }
    return false;
  }

  const std::string signature() const override {
    std::stringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << precision_->value() << "," << scale_->value();
    signature << ">";
    return signature.str();
  }

  const std::string precision() const {
    return precision_->value();
  }

  const std::string scale() const {
    return scale_->value();
  }

 private:
  SubstraitStringLiteralTypePtr precision_;
  SubstraitStringLiteralTypePtr scale_;
};

class SubstraitFixedBinaryType
    : public SubstraitTypeBase<SubstraitTypeKind ::kFixedBinary> {
 public:
  SubstraitFixedBinaryType(const SubstraitStringLiteralTypePtr& length)
      : length_(length) {}

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitFixedBinaryType>(other)) {
      return type->length_ == length_;
    }
    return false;
  }

  const std::string signature() const override {
    std::stringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << length_->value();
    signature << ">";
    return signature.str();
  }

 protected:
  SubstraitStringLiteralTypePtr length_;
};

class SubstraitFixedCharType
    : public SubstraitTypeBase<SubstraitTypeKind ::kFixedChar> {
 public:
  SubstraitFixedCharType(const SubstraitStringLiteralTypePtr& length)
      : length_(length) {}

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitFixedCharType>(other)) {
      return type->length_ == length_;
    }
    return false;
  }

  const std::string signature() const override {
    std::ostringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << length_->value();
    signature << ">";
    return signature.str();
  }

 protected:
  SubstraitStringLiteralTypePtr length_;
};

class SubstraitVarcharType
    : public SubstraitTypeBase<SubstraitTypeKind ::kVarchar> {
 public:
  SubstraitVarcharType(const SubstraitStringLiteralTypePtr& length)
      : length_(length) {}

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitVarcharType>(other)) {
      return type->length_ == length_;
    }
    return false;
  }

  const std::string signature() const override {
    std::ostringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << length_->value();
    signature << ">";
    return signature.str();
  }

 protected:
  SubstraitStringLiteralTypePtr length_;
};

class SubstraitListType : public SubstraitTypeBase<SubstraitTypeKind ::kList> {
 public:
  SubstraitListType(const SubstraitTypePtr& child) : type_(child){};

  const SubstraitTypePtr type() const {
    return type_;
  }

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitListType>(other)) {
      return type->type_->isSameAs(type_);
    }
    return false;
  }

  const std::string signature() const override {
    std::ostringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << type_->signature();
    signature << ">";
    return signature.str();
  }

 private:
  SubstraitTypePtr type_;
};

class SubstraitStructType
    : public SubstraitTypeBase<SubstraitTypeKind ::kStruct> {
 public:
  SubstraitStructType(const std::vector<SubstraitTypePtr>& types)
      : children_(types) {}

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitStructType>(other)) {
      bool sameSize = type->children_.size() == children_.size();
      if (sameSize) {
        for (int i = 0; i < children_.size(); i++) {
          if (!children_[i]->isSameAs(type->children_[i])) {
            return false;
          }
        }
        return true;
      }
    }
    return false;
  }

  const std::string signature() const override {
    std::ostringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    for (auto it = children_.begin(); it != children_.end(); ++it) {
      const auto& typeSign = (*it)->signature();
      if (it == children_.end() - 1) {
        signature << typeSign;
      } else {
        signature << typeSign << ",";
      }
    }
    signature << ">";
    return signature.str();
  }

 private:
  std::vector<SubstraitTypePtr> children_;
};

class SubstraitMapType : public SubstraitTypeBase<SubstraitTypeKind ::kMap> {
 public:
  SubstraitMapType(
      const SubstraitTypePtr& keyType,
      const SubstraitTypePtr& valueType)
      : keyType_(keyType), valueType_(valueType) {}

  const SubstraitTypePtr keyType() const {
    return keyType_;
  }

  const SubstraitTypePtr valueType() const {
    return valueType_;
  }

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitMapType>(other)) {
      return type->keyType_->isSameAs(keyType_) &&
          type->valueType_->isSameAs(valueType_);
    }
    return false;
  }

  const std::string signature() const override {
    std::ostringstream signature;
    signature << SubstraitTypeBase::signature();
    signature << "<";
    signature << keyType_->signature();
    signature << ",";
    signature << valueType_->signature();
    signature << ">";
    return signature.str();
  }

 private:
  SubstraitTypePtr keyType_;
  SubstraitTypePtr valueType_;
};

class SubstraitUsedDefinedType
    : public SubstraitTypeBase<SubstraitTypeKind ::kUserDefined> {
 public:
  SubstraitUsedDefinedType(const std::string& value) : value_(value) {}

  const std::string& value() const {
    return value_;
  }

  bool isSameAs(
      const std::shared_ptr<const SubstraitType>& other) const override {
    if (const auto& type =
            std::dynamic_pointer_cast<const SubstraitUsedDefinedType>(other)) {
      return type->value_ == value_;
    }
    return false;
  }

  const bool isUnknown() const override {
    return "unknown" == value_;
  }

 private:
  /// raw string of wildcard type.
  const std::string value_;
};

struct SubstraitTypeAnchor {
  std::string uri;
  std::string name;

  bool operator==(const SubstraitTypeAnchor& other) const {
    return (uri == other.uri && name == other.name);
  }
};

template <TypeKind T>
class SubstraitTypeCreator {};

#define SUBSTRAIT_TYPE_OF(typeKind)                                  \
  std::make_shared<SubstraitTypeBase<SubstraitTypeKind ::typeKind>>( \
      SubstraitTypeBase<SubstraitTypeKind ::typeKind>())

template <>
class SubstraitTypeCreator<TypeKind::BOOLEAN> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kBool);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TINYINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kI8);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::SMALLINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kI16);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTEGER> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kI32);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::BIGINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kI64);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::REAL> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kFp32);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DOUBLE> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kFp64);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARCHAR> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kVarchar);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARBINARY> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kBinary);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TIMESTAMP> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kTimestamp);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DATE> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kDate);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTERVAL_DAY_TIME> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = SUBSTRAIT_TYPE_OF(kIntervalDay);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::SHORT_DECIMAL> {
 public:
  static std::shared_ptr<const SubstraitType> create(const TypePtr& iType) {
    const auto& decimalType =
        std::dynamic_pointer_cast<const DecimalType<TypeKind::SHORT_DECIMAL>>(
            iType);

    static const auto type = std::make_shared<SubstraitDecimalType>(
        std::to_string(decimalType->precision()),
        std::to_string(decimalType->scale()));
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::LONG_DECIMAL> {
 public:
  static std::shared_ptr<const SubstraitType> create(const TypePtr& iType) {
    const auto& decimalType =
        std::dynamic_pointer_cast<const DecimalType<TypeKind::LONG_DECIMAL>>(
            iType);
    static const auto type = std::make_shared<SubstraitDecimalType>(
        std::to_string(decimalType->precision()),
        std::to_string(decimalType->scale()));
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ARRAY> {
 public:
  static std::shared_ptr<SubstraitListType> create(const TypePtr& iType) {
    const auto& arrayType = std::dynamic_pointer_cast<const ArrayType>(iType);
    static const auto type = std::make_shared<SubstraitListType>(
        SubstraitType::fromVelox(arrayType->elementType()));
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::MAP> {
 public:
  static std::shared_ptr<SubstraitMapType> create(const TypePtr& iType) {
    const auto& mapType = std::dynamic_pointer_cast<const MapType>(iType);
    static const auto type = std::make_shared<SubstraitMapType>(
        SubstraitType::fromVelox(mapType->keyType()),
        SubstraitType::fromVelox(mapType->valueType()));
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ROW> {
 public:
  static std::shared_ptr<SubstraitStructType> create(const TypePtr& iType) {
    const auto& rowType = std::dynamic_pointer_cast<const RowType>(iType);

    std::vector<SubstraitTypePtr> types;
    types.reserve(rowType->size());
    for (const auto& type : rowType->children()) {
      types.emplace_back(SubstraitType::fromVelox(type));
    }
    static const auto type = std::make_shared<SubstraitStructType>(types);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::UNKNOWN> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type =
        std::make_shared<SubstraitUsedDefinedType>("unknown");
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::FUNCTION> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    VELOX_NYI("FUNCTION type not supported.");
  }
};

template <>
class SubstraitTypeCreator<TypeKind::OPAQUE> {
 public:
  static SubstraitTypePtr create(TypePtr& iType) {
    VELOX_NYI("OPAQUE type not supported.");
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INVALID> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    VELOX_NYI("Invalid type not supported.");
  }
};

template <SubstraitTypeKind KIND>
class SubstraitScalarType : public SubstraitTypeBase<KIND> {};

#define SUBSTRAIT_SCALAR_TYPE_MAPPING(typeKind)                           \
  {                                                                       \
    SubstraitTypeTraits<SubstraitTypeKind::typeKind>::typeString,         \
        std::make_shared<SubstraitTypeBase<SubstraitTypeKind::typeKind>>( \
            SubstraitTypeBase<SubstraitTypeKind::typeKind>())             \
  }

using SubstraitTypeAnchorPtr = std::shared_ptr<SubstraitTypeAnchor>;

} // namespace facebook::velox::substrait

namespace std {
/// hash function of facebook::velox::substrait::SubstraitTypeAnchor
template <>
struct hash<facebook::velox::substrait::SubstraitTypeAnchor> {
  size_t operator()(
      const facebook::velox::substrait::SubstraitTypeAnchor& k) const {
    return hash<std::string>()(k.name) ^ hash<std::string>()(k.uri);
  }
};

}; // namespace std