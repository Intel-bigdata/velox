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

#include "velox/common/base/Exceptions.h"
#include "velox/core/Expressions.h"
#include "velox/substrait/SubstraitExtension.h"
#include "velox/substrait/SubstraitFunctionMappings.h"
#include "velox/substrait/SubstraitParser.h"
#include "velox/substrait/SubstraitSignature.h"
#include "velox/substrait/VeloxToSubstraitType.h"
#include "velox/type/Type.h"

namespace facebook::velox::substrait {

class SubstraitFunctionLookup {
 protected:
  SubstraitFunctionLookup(
      const bool& forAggregateFunc,
      const std::vector<SubstraitFunctionVariantPtr>& functionVariants,
      const SubstraitFunctionMappingsPtr& functionMappings);

 public:
  /// lookup function variant by given substrait function Signature.
  const std::optional<SubstraitFunctionVariantPtr> lookupFunction(
      const SubstraitSignaturePtr& functionSignature) const;

 protected:
  /// get the map which store the function names in difference between velox
  /// and substrait.
  virtual const FunctionMappings getFunctionMappings() const = 0;

  const SubstraitFunctionMappingsPtr functionMappings_;

 private:
  /// A wrapper class wrap a function variant which contains wildcard type.
  class WildcardFunctionVariant {
   public:
    WildcardFunctionVariant(const SubstraitFunctionVariantPtr& functionVaraint);

    ///  return function varaint if current wildcard function variant match the
    ///  given signature and.
    std::optional<SubstraitFunctionVariantPtr> tryMatch(
        const SubstraitSignaturePtr& signature) const;

   private:
    /// test current wildcard function variant match the given signature.
    bool isSameTypeTraits(const SubstraitSignaturePtr& signature) const;

    /// A map store type position and its type reference.
    std::unordered_map<int, int> typeTraits;

    /// the underlying function variant;
    const SubstraitFunctionVariantPtr underlying_;
  };

  using FunctionTypeTraitPtr = std::shared_ptr<const WildcardFunctionVariant>;

  class SubstraitFunctionFinder {
   public:
    /// construct FunctionFinder with function name and it's function variants
    SubstraitFunctionFinder(
        const std::string& name,
        const bool& forAggregateFunc,
        const std::vector<SubstraitFunctionVariantPtr>& functionVariants);

    /// lookup function variant by given substrait function signature.
    const std::optional<SubstraitFunctionVariantPtr> lookupFunction(
        const SubstraitSignaturePtr& signature) const;

   private:
    /// function name
    const std::string name_;
    const bool forAggregateFunc_;
    /// A map store the function signature and corresponding function variant
    std::unordered_map<std::string, SubstraitFunctionVariantPtr> directMap_;
    /// A map store the intermediate function signature and corresponding
    /// function variant
    std::unordered_map<std::string, SubstraitFunctionVariantPtr>
        intermediateMap_;
    /// A collection of function variant which contains wildcard type
    std::vector<FunctionTypeTraitPtr> wildcardFunctionVariants_;
  };

  using SubstraitFunctionFinderPtr =
      std::shared_ptr<const SubstraitFunctionFinder>;

  std::unordered_map<std::string, SubstraitFunctionFinderPtr>
      functionSignatures_;
};

class SubstraitScalarFunctionLookup : public SubstraitFunctionLookup {
 public:
  SubstraitScalarFunctionLookup(
      const SubstraitExtensionPtr& extension,
      const SubstraitFunctionMappingsPtr& functionMappings)
      : SubstraitFunctionLookup(
            false,
            extension->scalarFunctionVariants,
            functionMappings) {}

 protected:
  /// A  map store the difference of scalar function names between velox
  /// and substrait.
  const FunctionMappings getFunctionMappings() const override {
    return functionMappings_->scalarMappings();
  }
};

using SubstraitScalarFunctionLookupPtr =
    std::shared_ptr<const SubstraitScalarFunctionLookup>;

class SubstraitAggregateFunctionLookup : public SubstraitFunctionLookup {
 public:
  SubstraitAggregateFunctionLookup(
      const SubstraitExtensionPtr& extension,
      const SubstraitFunctionMappingsPtr& functionMappings)
      : SubstraitFunctionLookup(
            true,
            extension->aggregateFunctionVariants,
            functionMappings) {}

 protected:
  /// A  map store the difference of aggregate function names between velox
  /// and substrait.
  const FunctionMappings getFunctionMappings() const override {
    return functionMappings_->aggregateMappings();
  }
};

using SubstraitAggregateFunctionLookupPtr =
    std::shared_ptr<const SubstraitAggregateFunctionLookup>;

} // namespace facebook::velox::substrait
