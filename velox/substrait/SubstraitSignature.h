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

#include "SubstraitType.h"
#include "velox/substrait/SubstraitFunctionMappings.h"

namespace facebook::velox::substrait {

class SubstraitSignature final {
 public:
  /// construct the substrait function signature with function name, return type
  /// and arguments.
  SubstraitSignature(
      const std::string& name,
      const SubstraitTypePtr& returnType,
      const std::vector<SubstraitTypePtr>& arguments)
      : name_(name), returnType_(returnType), arguments_(arguments) {}

  /// construct the substrait function signature with function name, return
  /// type.
  SubstraitSignature(
      const std::string& name,
      const SubstraitTypePtr& returnType)
      : name_(name), returnType_(returnType) {}

  static std::shared_ptr<SubstraitSignature> of(
      const std::string& name,
      const SubstraitTypePtr& returnType) {
    return std::make_shared<SubstraitSignature>(name, returnType);
  }

  static std::shared_ptr<SubstraitSignature> of(
      const std::string& name,
      const SubstraitTypePtr& returnType,
      const std::vector<SubstraitTypePtr>& arguments) {
    return std::make_shared<SubstraitSignature>(name, returnType, arguments);
  }

  /// Return function signature according to the given function name and
  /// substrait types.
  const std::string signature() const;

  const std::string& getName() const {
    return name_;
  }

  const std::vector<SubstraitTypePtr>& getArguments() const {
    return arguments_;
  }

  const SubstraitTypePtr& getReturnType() const {
    return returnType_;
  }

  /// return an new function signature with function mappings
  static const std::string signature(
      const std::string& functionSignature,
      const SubstraitFunctionMappingsPtr& functionMappings);

 private:
  const std::string name_;
  const SubstraitTypePtr returnType_;
  const std::vector<SubstraitTypePtr> arguments_;
};

using SubstraitSignaturePtr = std::shared_ptr<const SubstraitSignature>;

} // namespace facebook::velox::substrait
