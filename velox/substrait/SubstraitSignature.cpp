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

#include "velox/substrait/SubstraitSignature.h"

namespace facebook::velox::substrait {

const std::string SubstraitFunctionSignature::signature() const {
  std::ostringstream signature;
  signature << name_;
  if (!arguments_.empty()) {
    signature << ":";
    for (auto it = arguments_.begin(); it != arguments_.end(); ++it) {
      const auto& typeSign = (*it)->signature();
      if (it == arguments_.end() - 1) {
        signature << typeSign;
      } else {
        signature << typeSign << "_";
      }
    }
  }

  return signature.str();
}

const std::string SubstraitFunctionSignature::signature(
    const std::string& functionSignature,
    const SubstraitFunctionMappingsPtr& functionMappings) {
  // try to replace function name with function mappings
  if (functionMappings) {
    std::vector<std::string> functionAndSignatures;
    folly::split(":", functionSignature, functionAndSignatures);
    const auto& scalarMappings = functionMappings->scalarMappings();
    const auto& aggregateMappings = functionMappings->aggregateMappings();
    if (functionAndSignatures.size() == 2) {
      const auto& functionName = functionAndSignatures.at(0);
      const auto& signatures = functionAndSignatures.at(1);
      if (scalarMappings.find(functionName) != scalarMappings.end()) {
        return scalarMappings.at(functionName) + ":" + signatures;
      } else if (
          aggregateMappings.find(functionName) != aggregateMappings.end()) {
        return aggregateMappings.at(functionName) + ":" + signatures;
      }
    } else if (functionAndSignatures.size() == 1) {
      if (scalarMappings.find(functionSignature) != scalarMappings.end()) {
        return scalarMappings.at(functionSignature);
      } else if (
          aggregateMappings.find(functionSignature) !=
          aggregateMappings.end()) {
        return aggregateMappings.at(functionSignature);
      }
    }
  }
  return functionSignature;
}

} // namespace facebook::velox::substrait
