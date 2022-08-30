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

#include "velox/substrait/TypeUtils.h"
#include "velox/type/Type.h"

namespace facebook::velox::substrait {
std::vector<std::string_view> getTypesFromCompoundName(
    std::string_view compoundName) {
  // CompoundName is like ARRAY<BIGINT> or MAP<BIGINT,DOUBLE>
  // or ROW<BIGINT,ROW<DOUBLE,BIGINT>,ROW<DOUBLE,BIGINT>>
  // the position of then delimiter is where the number of leftAngleBracket
  // equals rightAngleBracket need to split.
  std::vector<std::string_view> types;
  std::vector<int> angleBracketNumEqualPos;
  auto leftAngleBracketPos = compoundName.find("<");
  auto rightAngleBracketPos = compoundName.rfind(">");
  auto typesName = compoundName.substr(
      leftAngleBracketPos + 1, rightAngleBracketPos - leftAngleBracketPos - 1);
  int leftAngleBracketNum = 0;
  int rightAngleBracketNum = 0;
  for (auto index = 0; index < typesName.length(); index++) {
    if (typesName[index] == '<') {
      leftAngleBracketNum++;
    }
    if (typesName[index] == '>') {
      rightAngleBracketNum++;
    }
    if (typesName[index] == ',' &&
        rightAngleBracketNum == leftAngleBracketNum) {
      angleBracketNumEqualPos.push_back(index);
    }
  }
  int startPos = 0;
  for (auto delimeterPos : angleBracketNumEqualPos) {
    types.emplace_back(typesName.substr(startPos, delimeterPos - startPos));
    startPos = delimeterPos + 1;
  }
  types.emplace_back(std::string_view(
      typesName.data() + startPos, typesName.length() - startPos));
  return types;
}

// TODO Refactor using Bison.
std::string_view getNameBeforeDelimiter(
    const std::string& compoundName,
    const std::string& delimiter) {
  std::size_t pos = compoundName.find(delimiter);
  if (pos == std::string::npos) {
    return compoundName;
  }
  return std::string_view(compoundName.data(), pos);
}

TypePtr toVeloxType(const std::string& typeName) {
  VELOX_CHECK(!typeName.empty(), "Cannot convert empty string to Velox type.");

  auto type = getNameBeforeDelimiter(typeName, "<");
  auto typeKind = mapNameToTypeKind(std::string(type));
  switch (typeKind) {
    case TypeKind::BOOLEAN:
      return BOOLEAN();
    case TypeKind::TINYINT:
      return TINYINT();
    case TypeKind::SMALLINT:
      return SMALLINT();
    case TypeKind::INTEGER:
      return INTEGER();
    case TypeKind::BIGINT:
      return BIGINT();
    case TypeKind::REAL:
      return REAL();
    case TypeKind::DOUBLE:
      return DOUBLE();
    case TypeKind::VARCHAR:
      return VARCHAR();
    case TypeKind::VARBINARY:
      return VARBINARY();
    case TypeKind::ARRAY: {
      auto fieldTypes = getTypesFromCompoundName(typeName);
      VELOX_CHECK_EQ(
          fieldTypes.size(), 1, "The size of ARRAY type should be only one.");
      return ARRAY(toVeloxType(std::string(fieldTypes[0])));
    }
    case TypeKind::MAP: {
      auto fieldTypes = getTypesFromCompoundName(typeName);
      VELOX_CHECK_EQ(
          fieldTypes.size(), 2, "The size of MAP type should be two.");
      auto keyType = toVeloxType(std::string(fieldTypes[0]));
      auto valueType = toVeloxType(std::string(fieldTypes[1]));
      return MAP(keyType, valueType);
    }
    case TypeKind::ROW: {
      auto fieldTypes = getTypesFromCompoundName(typeName);
      VELOX_CHECK(
          !fieldTypes.empty(),
          "Converting empty ROW type from Substrait to Velox is not supported.");

      std::vector<TypePtr> types;
      std::vector<std::string> names;
      for (int idx = 0; idx < fieldTypes.size(); idx++) {
        names.emplace_back("col_" + std::to_string(idx));
        types.emplace_back(toVeloxType(std::string(fieldTypes[idx])));
      }
      return ROW(std::move(names), std::move(types));
    }
    case TypeKind::UNKNOWN:
      return UNKNOWN();
    default:
      VELOX_NYI("Velox type conversion not supported for type {}.", typeName);
  }
}

template <TypeKind T>
class SubstraitTypeCreator;

/// template method for create SubstraitType by velox TypeKind.
template <TypeKind T>
std::shared_ptr<const SubstraitType> fromVeloxType(const TypePtr& type) {
  return SubstraitTypeCreator<T>::create(type);
}

SubstraitTypePtr fromVelox(const TypePtr& type) {
  if (type) {
    return VELOX_DYNAMIC_TYPE_DISPATCH(fromVeloxType, type->kind(), type);
  }
  return nullptr;
}

template <>
class SubstraitTypeCreator<TypeKind::BOOLEAN> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kBool();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TINYINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kI8();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::SMALLINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kI16();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTEGER> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kI32();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::BIGINT> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kI64();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::REAL> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kFp32();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DOUBLE> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kFp64();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARCHAR> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kString();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::VARBINARY> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kBinary();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::TIMESTAMP> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kTimestamp();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::DATE> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kDate();
    return type;
  }
};

template <>
class SubstraitTypeCreator<TypeKind::INTERVAL_DAY_TIME> {
 public:
  static SubstraitTypePtr create(const TypePtr& iType) {
    static const auto type = kIntervalDay();
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

    return std::make_shared<SubstraitDecimalType>(
        std::to_string(decimalType->precision()),
        std::to_string(decimalType->scale()));
  }
};

template <>
class SubstraitTypeCreator<TypeKind::LONG_DECIMAL> {
 public:
  static std::shared_ptr<const SubstraitType> create(const TypePtr& iType) {
    const auto& decimalType =
        std::dynamic_pointer_cast<const DecimalType<TypeKind::LONG_DECIMAL>>(
            iType);
    return std::make_shared<SubstraitDecimalType>(
        std::to_string(decimalType->precision()),
        std::to_string(decimalType->scale()));
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ARRAY> {
 public:
  static std::shared_ptr<const SubstraitType> create(const TypePtr& iType) {
    const auto& arrayType = std::dynamic_pointer_cast<const ArrayType>(iType);
    return std::make_shared<SubstraitListType>(
        fromVelox(arrayType->elementType()));
  }
};

template <>
class SubstraitTypeCreator<TypeKind::MAP> {
 public:
  static std::shared_ptr<const SubstraitType> create(const TypePtr& iType) {
    const auto& mapType = std::dynamic_pointer_cast<const MapType>(iType);
    return std::make_shared<SubstraitMapType>(
        fromVelox(mapType->keyType()), fromVelox(mapType->valueType()));
  }
};

template <>
class SubstraitTypeCreator<TypeKind::ROW> {
 public:
  static std::shared_ptr<SubstraitStructType> create(const TypePtr& iType) {
    const auto& rowType = std::dynamic_pointer_cast<const RowType>(iType);

    std::vector<SubstraitTypePtr> types;
    for (const auto& type : rowType->children()) {
      const auto& substraitType = fromVelox(type);
      types.emplace_back(substraitType);
    }
    return std::make_shared<SubstraitStructType>(types);
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

} // namespace facebook::velox::substrait
