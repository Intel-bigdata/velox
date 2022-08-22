/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
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
  static constexpr const char* matchingType = "boolean";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI8> {
  static constexpr const char* signature = "i8";
  static constexpr const char* matchingType = "i8";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI16> {
  static constexpr const char* signature = "i16";
  static constexpr const char* matchingType = "i16";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI32> {
  static constexpr const char* signature = "i32";
  static constexpr const char* matchingType = "i32";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kI64> {
  static constexpr const char* signature = "i64";
  static constexpr const char* matchingType = "i64";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFp32> {
  static constexpr const char* signature = "fp32";
  static constexpr const char* matchingType = "fp32";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFp64> {
  static constexpr const char* signature = "fp64";
  static constexpr const char* matchingType = "fp64";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kString> {
  static constexpr const char* signature = "str";
  static constexpr const char* matchingType = "string";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kBinary> {
  static constexpr const char* signature = "vbin";
  static constexpr const char* matchingType = "binary";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTimestamp> {
  static constexpr const char* signature = "ts";
  static constexpr const char* matchingType = "timestamp";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTimestampTz> {
  static constexpr const char* signature = "tstz";
  static constexpr const char* matchingType = "timestamp_tz";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kDate> {
  static constexpr const char* signature = "date";
  static constexpr const char* matchingType = "date";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kTime> {
  static constexpr const char* signature = "time";
  static constexpr const char* matchingType = "time";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kIntervalYear> {
  static constexpr const char* signature = "iyear";
  static constexpr const char* matchingType = "interval_year";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kIntervalDay> {
  static constexpr const char* signature = "iday";
  static constexpr const char* matchingType = "interval_day";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kUuid> {
  static constexpr const char* signature = "uuid";
  static constexpr const char* matchingType = "uuid";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFixedChar> {
  static constexpr const char* signature = "fchar";
  static constexpr const char* matchingType = "fixedchar";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kVarchar> {
  static constexpr const char* signature = "vchar";
  static constexpr const char* matchingType = "varchar";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kFixedBinary> {
  static constexpr const char* signature = "fbin";
  static constexpr const char* matchingType = "fixedbinary";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kDecimal> {
  static constexpr const char* signature = "dec";
  static constexpr const char* matchingType = "decimal";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kStruct> {
  static constexpr const char* signature = "struct";
  static constexpr const char* matchingType = "struct";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kList> {
  static constexpr const char* signature = "list";
  static constexpr const char* matchingType = "list";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kMap> {
  static constexpr const char* signature = "map";
  static constexpr const char* matchingType = "map";
};

template <>
struct SubstraitTypeTraits<SubstraitTypeKind::kUserDefined> {
  static constexpr const char* signature = "u!name";
  static constexpr const char* matchingType = "user defined type";
};

template <TypeKind T>
class SubstraitTypeCreator;

class SubstraitType {
 public:
  /// deserialize substrait raw type string into Substrait extension  type.
  /// @param rawType - substrait extension raw string type
  static std::shared_ptr<const SubstraitType> decode(const std::string rawType);

  /// Return the Substrait extension type  according to the velox type.
  static std::shared_ptr<const SubstraitType> fromVelox(const TypePtr& type) {
    return VELOX_DYNAMIC_TYPE_DISPATCH(substraitTypeMaker, type->kind());
  }

  /// template class for create SubstraitType by given velox TypeKind
  template <TypeKind T>
  static std::shared_ptr<const SubstraitType> substraitTypeMaker() {
    return SubstraitTypeCreator<T>::create();
  }

  /// signature name of substrait type.
  virtual const char* signature() const = 0;

  /// test type is a Wildcard type or not.
  virtual const bool isWildcard() const {
    return false;
  }

  /// substrait type prefix used to convert yaml type
  virtual const char* matchingType() const = 0;

  /// unknown type,see @SubstraitUnknownType
  virtual const bool isUnknown() const {
    return false;
  }

  /// a known substrait type kind
  virtual const bool isKind() const {
    return !isWildcard() && !isUnknown();
  }

 private:
  /// A map store the raw type string and corresponding Substrait Type
  static const std::
      unordered_map<std::string, std::shared_ptr<const SubstraitType>>&
      typeMappings();
};

using SubstraitTypePtr = std::shared_ptr<const SubstraitType>;

using SubstraitTypePtr = std::shared_ptr<const SubstraitType>;

/// Types used in function argument declarations.
template <SubstraitTypeKind Kind>
class SubstraitTypeBase : public SubstraitType {
 public:
  const char* signature() const override {
    return SubstraitTypeTraits<Kind>::signature;
  }
  const char* matchingType() const override {
    return SubstraitTypeTraits<Kind>::matchingType;
  }
  const SubstraitTypeKind kind() const {
    return Kind;
  }
};

class SubstraitWildcardType : public SubstraitType {
 public:
  SubstraitWildcardType(const std::string& value) : value_(value) {}
  const char* signature() const override {
    return value_.c_str();
  }
  const bool isWildcard() const override {
    return true;
  }

  const char* matchingType() const override {
    return "any";
  }
  const std::string& value() const {
    return value_;
  }

 private:
  /// raw string of wildcard type.
  const std::string value_;
};

class SubstraitUnknownType : public SubstraitType {
 public:
  const char* signature() const override {
    return "unknown";
  }
  const bool isUnknown() const override {
    return true;
  }

  const char* matchingType() const override {
    return "unknown";
  }
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
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kBool);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TINYINT> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kI8);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::SMALLINT> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kI16);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTEGER> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kI32);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::BIGINT> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kI64);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::REAL> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kFp32);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DOUBLE> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kFp64);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARCHAR> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kVarchar);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARBINARY> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kBinary);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TIMESTAMP> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kTimestamp);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DATE> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kDate);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTERVAL_DAY_TIME> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kIntervalDay);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::SHORT_DECIMAL> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kDecimal);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::LONG_DECIMAL> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kDecimal);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ARRAY> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kList);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::MAP> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kMap);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ROW> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = SUBSTRAIT_TYPE_OF(kStruct);
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::UNKNOWN> {
 public:
  static SubstraitTypePtr create() {
    static const auto type = std::make_shared<SubstraitUnknownType>();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::FUNCTION> {
 public:
  static SubstraitTypePtr create() {
    VELOX_NYI("FUNCTION type not supported.");
  }
};

template <>
class SubstraitTypeCreator<TypeKind::OPAQUE> {
 public:
  static SubstraitTypePtr create() {
    VELOX_NYI("OPAQUE type not supported.");
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INVALID> {
 public:
  static SubstraitTypePtr create() {
    VELOX_NYI("Invalid type not supported.");
  }
};

#define SUBSTRAIT_SCALAR_TYPE_MAPPING(typeKind)                           \
  {                                                                       \
    SubstraitTypeTraits<SubstraitTypeKind::typeKind>::matchingType,       \
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