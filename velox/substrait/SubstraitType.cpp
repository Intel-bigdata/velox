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

#include "velox/substrait/SubstraitType.h"

namespace facebook::velox::substrait {

size_t findNextComma(const std::string& str, size_t start) {
  int cnt = 0;
  for (auto i = start; i < str.size(); i++) {
    if (str[i] == '<') {
      cnt++;
    } else if (str[i] == '>') {
      cnt--;
    } else if (cnt == 0 && str[i] == ',') {
      return i;
    }
  }

  return std::string::npos;
}

SubstraitTypePtr SubstraitType::decode(const std::string& rawType) {
  const auto& parenPos = rawType.find('<');
  std::string matchingType = rawType;
  std::transform(
      matchingType.begin(),
      matchingType.end(),
      matchingType.begin(),
      [](unsigned char c) { return std::tolower(c); });

  if (parenPos == std::string::npos) {
    const auto& scalarTypes = scalarTypeMapping();
    if (scalarTypes.find(matchingType) != scalarTypes.end()) {
      return scalarTypes.at(matchingType);
    } else if (matchingType.rfind("unknown", 0) == 0) {
      return std::make_shared<const SubstraitUsedDefinedType>(rawType);
    } else {
      return std::make_shared<const SubstraitStringLiteralType>(rawType);
    }
  }

  const auto& endParenPos = rawType.rfind('>');
  VELOX_CHECK(
      endParenPos != std::string::npos,
      "Couldn't find the closing parenthesis.");

  std::vector<SubstraitTypePtr> nestedTypes;
  auto baseType = matchingType.substr(0, parenPos);
  auto prevPos = parenPos + 1;
  auto commaPos = findNextComma(rawType, prevPos);
  while (commaPos != std::string::npos) {
    auto token = rawType.substr(prevPos, commaPos - prevPos);
    nestedTypes.emplace_back(SubstraitType::decode(token));
    prevPos = commaPos + 1;
    commaPos = findNextComma(rawType, prevPos);
  }

  auto token = rawType.substr(prevPos, endParenPos - prevPos);
  nestedTypes.emplace_back(decode(token));

  if (baseType == "list") {
    VELOX_CHECK(
        nestedTypes.size() == 1,
        "list type can only have one parameterized type");
    return std::make_shared<SubstraitListType>(nestedTypes[0]);
  } else if (baseType == "map") {
    VELOX_CHECK(
        nestedTypes.size() == 2,
        "map type must have a parameterized type for key and a parameterized type for value");
    return std::make_shared<SubstraitMapType>(nestedTypes[0], nestedTypes[1]);
  } else if (baseType == "decimal") {
    VELOX_CHECK(
        nestedTypes.size() == 2,
        "decimal type must have a parameterized type for scale and a parameterized type for precision");
    auto precision =
        std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
            nestedTypes[0]);
    auto scale = std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
        nestedTypes[1]);
    return std::make_shared<SubstraitDecimalType>(precision, scale);
  } else if (baseType == "varchar") {
    VELOX_CHECK(
        nestedTypes.size() == 1,
        "varchar type must have a parameterized type length");
    auto length = std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
        nestedTypes[0]);
    return std::make_shared<SubstraitVarcharType>(length);
  } else if (baseType == "fixedchar") {
    VELOX_CHECK(
        nestedTypes.size() == 1,
        "fixedchar type must have a parameterized type length");
    auto length = std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
        nestedTypes[0]);
    return std::make_shared<SubstraitFixedCharType>(length);
  } else if (baseType == "fixedbinary") {
    VELOX_CHECK(
        nestedTypes.size() == 1,
        "fixedbinary type must have a parameterized type length");
    auto length = std::dynamic_pointer_cast<const SubstraitStringLiteralType>(
        nestedTypes[0]);
    return std::make_shared<SubstraitFixedBinaryType>(length);
  } else if (baseType == "struct") {
    VELOX_CHECK(
        !nestedTypes.empty(),
        "struct type must have at least one parameterized type");
    return std::make_shared<SubstraitStructType>(nestedTypes);
  } else {
    VELOX_NYI("Unsupported typed {}", rawType);
  }
}

const std::unordered_map<std::string, SubstraitTypePtr>&
SubstraitType::scalarTypeMapping() {
  static const std::unordered_map<std::string, SubstraitTypePtr> scalarTypeMap{
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kBool),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kI8),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kI16),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kI32),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kI64),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kFp32),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kFp64),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kString),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kBinary),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kTimestamp),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kTimestampTz),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kDate),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kTime),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kIntervalDay),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kIntervalYear),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kUuid),
  };
  return scalarTypeMap;
}

} // namespace facebook::velox::substrait
