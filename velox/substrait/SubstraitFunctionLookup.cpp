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

#include "velox/substrait/SubstraitFunctionLookup.h"
#include <folly/String.h>
#include "velox/substrait/SubstraitSignature.h"

namespace facebook::velox::substrait {

SubstraitFunctionLookup::SubstraitFunctionLookup(
    const bool& forAggregateFunc,
    const std::vector<SubstraitFunctionVariantPtr>& functions,
    const SubstraitFunctionMappingsPtr& functionMappings)
    : functionMappings_(functionMappings) {
  std::unordered_map<std::string, std::vector<SubstraitFunctionVariantPtr>>
      signatures;

  for (const auto& function : functions) {
    if (signatures.find(function->name) == signatures.end()) {
      std::vector<SubstraitFunctionVariantPtr> nameFunctions;
      nameFunctions.emplace_back(function);
      signatures.insert({function->name, nameFunctions});
    } else {
      auto& nameFunctions = signatures.at(function->name);
      nameFunctions.emplace_back(function);
    }
  }

  for (const auto& [name, signature] : signatures) {
    auto functionFinder = std::make_shared<SubstraitFunctionFinder>(
        name, forAggregateFunc, signature);
    functionSignatures_.insert({name, functionFinder});
  }
}

const std::optional<SubstraitFunctionVariantPtr>
SubstraitFunctionLookup::lookupFunction(
    const SubstraitSignaturePtr& functionSignature) const {
  const auto& functionMappings = getFunctionMappings();
  const auto& functionName = functionSignature->getName();
  const auto& substraitFunctionName =
      functionMappings.find(functionName) != functionMappings.end()
      ? functionMappings.at(functionName)
      : functionName;

  if (functionSignatures_.find(substraitFunctionName) ==
      functionSignatures_.end()) {
    return std::nullopt;
  }
  const auto& newFunctionSignature = SubstraitFunctionSignature::of(
      substraitFunctionName,
      functionSignature->getArguments(),
      functionSignature->getReturnType());
  const auto& functionFinder = functionSignatures_.at(substraitFunctionName);
  return functionFinder->lookupFunction(newFunctionSignature);
}

SubstraitFunctionLookup::SubstraitFunctionFinder::SubstraitFunctionFinder(
    const std::string& name,
    const bool& forAggregateFunc,
    const std::vector<SubstraitFunctionVariantPtr>& functions)
    : name_(name), forAggregateFunc_(forAggregateFunc) {
  for (const auto& function : functions) {
    directMap_.insert({function->signature(), function});
    if (function->requiredArguments().size() != function->arguments.size()) {
      const std::string& functionKey = SubstraitFunctionVariant::signature(
          function->name, function->requiredArguments());
      directMap_.insert({functionKey, function});
    }

    if (const auto& aggregateFunc =
            std::dynamic_pointer_cast<const SubstraitAggregateFunctionVariant>(
                function)) {
      intermediateMap_.insert(
          {aggregateFunc->intermediateSignature(), function});
    }
  }

  for (const auto& function : functions) {
    if (function->hasWildcardArgument()) {
      const auto& wildcardFuncVariant =
          std::make_shared<WildcardFunctionVariant>(function);
      wildcardFunctionVariants_.emplace_back(wildcardFuncVariant);
    }
  }
}

const std::optional<SubstraitFunctionVariantPtr>
SubstraitFunctionLookup::SubstraitFunctionFinder::lookupFunction(
    const SubstraitSignaturePtr& functionSignature) const {
  const auto& types = functionSignature->getArguments();
  const auto& signature = functionSignature->signature();
  /// try to do a direct match
  if (directMap_.find(signature) != directMap_.end()) {
    const auto& functionVariant = directMap_.at(signature);
    const auto& returnType = functionSignature->getReturnType();
    if (returnType && functionSignature->getReturnType() &&
        returnType->isSameAs(functionSignature->getReturnType())) {
      return std::make_optional(functionVariant);
    }
    return std::nullopt;
  }

  // try to match with intermediate function signature if for aggregate function
  // lookup.
  if (forAggregateFunc_ &&
      intermediateMap_.find(signature) != intermediateMap_.end()) {
    const auto& functionVariant = intermediateMap_.at(signature);
    const auto& returnType = functionSignature->getReturnType();
    if (returnType && functionSignature->getReturnType() &&
        returnType->isSameAs(functionSignature->getReturnType())) {
      return std::make_optional(functionVariant);
    }
  }

  // return empty if no arguments
  if (functionSignature->getArguments().empty()) {
    return std::nullopt;
  }

  // try to match with wildcardFunctionVariants_.
  for (const auto& wildCardFunctionVariant : wildcardFunctionVariants_) {
    const auto& matched = wildCardFunctionVariant->tryMatch(functionSignature);
    if (matched.has_value()) {
      return matched;
    }
  }
  return std::nullopt;
}

SubstraitFunctionLookup::WildcardFunctionVariant::WildcardFunctionVariant(
    const SubstraitFunctionVariantPtr& functionVariant)
    : underlying_(functionVariant) {
  std::unordered_map<std::string, int> typeToRef;
  int typeRef = 0;
  int pos = 0;
  for (auto& arg : underlying_->arguments) {
    if (arg->isValueArgument()) {
      const auto& typeString = arg->toTypeString();
      if (typeToRef.find(typeString) == typeToRef.end()) {
        typeToRef.insert({typeString, typeRef++});
      }
      typeTraits.insert({pos++, typeToRef[typeString]});
    }
  }
}

std::optional<SubstraitFunctionVariantPtr>
SubstraitFunctionLookup::WildcardFunctionVariant ::tryMatch(
    const SubstraitSignaturePtr& signature) const {
  bool sameTypeTraits = isSameTypeTraits(signature);
  if (sameTypeTraits) {
    auto functionVariant = *underlying_.get();
    auto& arguments = functionVariant.arguments;
    arguments.clear();
    arguments.reserve(signature->getArguments().size());
    std::vector<SubstraitFunctionArgumentPtr> args;
    for (const auto& argument : signature->getArguments()) {
      const auto& valueArgument = std::make_shared<SubstraitValueArgument>();
      valueArgument->type = argument;
      arguments.emplace_back(valueArgument);
    }
    return std::make_shared<SubstraitFunctionVariant>(functionVariant);
  }
  return std::nullopt;
}

bool SubstraitFunctionLookup::WildcardFunctionVariant::isSameTypeTraits(
    const SubstraitSignaturePtr& signature) const {
  std::unordered_map<std::string, int> typeToRef;
  std::unordered_map<int, int> signatureTraits;
  int ref = 0;
  int pos = 0;
  for (auto& arg : signature->getArguments()) {
    const auto& typeString = arg->signature();
    if (typeToRef.find(typeString) == typeToRef.end()) {
      typeToRef.insert({typeString, ref++});
    }
    signatureTraits.insert({pos++, typeToRef[typeString]});
  }

  bool sameSize = typeTraits.size() == signatureTraits.size();
  if (sameSize) {
    for (const auto& [typePos, typeRef] : typeTraits) {
      if (signatureTraits.at(typePos) != typeRef) {
        return false;
      }
    }
    return true;
  } else {
    return false;
  }
}

} // namespace facebook::velox::substrait
