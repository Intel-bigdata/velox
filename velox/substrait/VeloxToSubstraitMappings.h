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

#include "velox/substrait/SubstraitFunctionLookup.h"

namespace facebook::velox::substrait {

class VeloxToSubstraitFunctionMappings : public SubstraitFunctionMappings {
 public:
  static const std::shared_ptr<VeloxToSubstraitFunctionMappings> make() {
    return std::make_shared<VeloxToSubstraitFunctionMappings>();
  }
  /// scalar function names in difference between  velox and Substrait.
  const FunctionMappings scalarMappings() const override {
    static const FunctionMappings scalarMappings{
        {"plus", "add"},
        {"minus", "subtract"},
        {"mod", "modulus"},
        {"eq", "equal"},
        {"neq", "not_equal"},
        {"substr", "substring"},
    };
    return scalarMappings;
  };

  /// aggregate function names in difference between velox and Substrait.
  const FunctionMappings aggregateMappings() const override {
    static const FunctionMappings aggregateMappings{};
    return aggregateMappings;
  };

  /// window function names in difference between velox and Substrait.
  const FunctionMappings windowMappings() const override {
    static const FunctionMappings aggregateMappings{};
    return aggregateMappings;
  };
};

using VeloxToSubstraitFunctionMappingsPtr =
    std::shared_ptr<VeloxToSubstraitFunctionMappings>;

} // namespace facebook::velox::substrait
