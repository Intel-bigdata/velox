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

#include "velox/substrait/SubstraitType.h"

namespace facebook::velox::substrait {

SubstraitTypePtr SubstraitType::decode(const std::string rawType) {
  std::string lowerCaseRawType = rawType;
  std::transform(
      lowerCaseRawType.begin(),
      lowerCaseRawType.end(),
      lowerCaseRawType.begin(),
      [](unsigned char c) { return std::tolower(c); });

  const auto& scalarTypes = typeMappings();
  if (scalarTypes.find(lowerCaseRawType) != scalarTypes.end()) {
    return scalarTypes.at(lowerCaseRawType);
  }
  for (const auto& [matchingType, substraitType] : scalarTypes) {
    if (lowerCaseRawType.rfind(matchingType, 0) == 0) {
      return substraitType;
    }
  }
  if (lowerCaseRawType.rfind("any", 0) == 0) {
    return std::make_shared<const SubstraitWildcardType>(rawType);

  } else if (lowerCaseRawType.rfind("unknown", 0) == 0) {
    return std::make_shared<const SubstraitUnknownType>();
  } else {
    VELOX_NYI(
        "Returning Substrait Type not supported for raw type {}.", rawType);
  }
}

const std::unordered_map<std::string, SubstraitTypePtr>&
SubstraitType::typeMappings() {
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
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kFixedChar),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kVarchar),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kFixedBinary),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kDecimal),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kStruct),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kList),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kMap),
      SUBSTRAIT_SCALAR_TYPE_MAPPING(kUserDefined),
  };
  return scalarTypeMap;
}

} // namespace facebook::velox::substrait
