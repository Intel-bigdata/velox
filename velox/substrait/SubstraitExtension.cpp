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

#include <yaml-cpp/yaml.h>
#include <sstream>

#include "velox/substrait/SubstraitExtension.h"
#include "velox/substrait/SubstraitSignature.h"

namespace YAML {

using namespace facebook::velox::substrait;

static bool decodeFunctionVariant(
    const Node& node,
    SubstraitFunctionVariant& function) {
  auto& returnType = node["return"];
  if (returnType && returnType.IsScalar()) {
    /// return type can be an expression
    const auto& returnExpr = returnType.as<std::string>();
    std::stringstream ss(returnExpr);
    std::string lastReturnType;
    while (std::getline(ss, lastReturnType, '\n')) {
    }
    function.returnType = SubstraitType::decode(lastReturnType);
    auto& args = node["args"];
    if (args && args.IsSequence()) {
      for (auto& arg : args) {
        if (arg["options"]) { // enum argument
          auto enumArgument = std::make_shared<SubstraitEnumArgument>(
              arg.as<SubstraitEnumArgument>());
          function.arguments.emplace_back(enumArgument);
        } else if (arg["value"]) { // value argument
          auto valueArgument = std::make_shared<SubstraitValueArgument>(
              arg.as<SubstraitValueArgument>());
          function.arguments.emplace_back(valueArgument);
        } else { // type argument
          auto typeArgument = std::make_shared<SubstraitTypeArgument>(
              arg.as<SubstraitTypeArgument>());
          function.arguments.emplace_back(typeArgument);
        }
      }
    }
    return true;
  }
  return false;
}

template <>
struct convert<SubstraitEnumArgument> {
  static bool decode(const Node& node, SubstraitEnumArgument& argument) {
    // 'options' is required property
    auto& options = node["options"];
    if (options && options.IsSequence()) {
      auto& required = node["required"];
      argument.required = required && required.as<bool>();
      return true;
    } else {
      return false;
    }
  }
};

template <>
struct convert<SubstraitValueArgument> {
  static bool decode(const Node& node, SubstraitValueArgument& argument) {
    auto& value = node["value"];
    if (value && value.IsScalar()) {
      auto valueType = value.as<std::string>();
      argument.type = SubstraitType::decode(valueType);
      return true;
    }
    return false;
  }
};

template <>
struct convert<SubstraitTypeArgument> {
  static bool decode(const Node& node, SubstraitTypeArgument& argument) {
    // no properties need to populate for type argument, just return true if
    // 'type' element exists.
    return node["type"];
  }
};

template <>
struct convert<SubstraitScalarFunctionVariant> {
  static bool decode(
      const Node& node,
      SubstraitScalarFunctionVariant& function) {
    return decodeFunctionVariant(node, function);
  };
};

template <>
struct convert<SubstraitAggregateFunctionVariant> {
  static bool decode(
      const Node& node,
      SubstraitAggregateFunctionVariant& function) {
    auto res = decodeFunctionVariant(node, function);
    if (res) {
      const auto& intermediate = node["intermediate"];
      if (intermediate) {
        function.intermediate =
            SubstraitType::decode(intermediate.as<std::string>());
      }
    }
    return res;
  }
};

template <>
struct convert<SubstraitScalarFunction> {
  static bool decode(const Node& node, SubstraitScalarFunction& function) {
    auto& name = node["name"];
    if (name && name.IsScalar()) {
      function.name = name.as<std::string>();
      auto& impls = node["impls"];
      if (impls && impls.IsSequence() && impls.size() > 0) {
        for (auto& impl : impls) {
          auto scalarFunctionVariant =
              impl.as<SubstraitScalarFunctionVariant>();
          scalarFunctionVariant.name = function.name;
          function.impls.emplace_back(
              std::make_shared<SubstraitScalarFunctionVariant>(
                  scalarFunctionVariant));
        }
      }
      return true;
    }
    return false;
  }
};

template <>
struct convert<SubstraitAggregateFunction> {
  static bool decode(const Node& node, SubstraitAggregateFunction& function) {
    auto& name = node["name"];
    if (name && name.IsScalar()) {
      function.name = name.as<std::string>();
      auto& impls = node["impls"];
      if (impls && impls.IsSequence() && impls.size() > 0) {
        for (auto& impl : impls) {
          auto aggregateFunctionVariant =
              impl.as<SubstraitAggregateFunctionVariant>();
          aggregateFunctionVariant.name = function.name;
          function.impls.emplace_back(
              std::make_shared<SubstraitAggregateFunctionVariant>(
                  aggregateFunctionVariant));
        }
      }
      return true;
    }
    return false;
  }
};

template <>
struct convert<facebook::velox::substrait::SubstraitTypeAnchor> {
  static bool decode(
      const Node& node,
      facebook::velox::substrait::SubstraitTypeAnchor& typeAnchor) {
    auto& name = node["name"];
    if (name && name.IsScalar()) {
      typeAnchor.name = name.as<std::string>();
      return true;
    }
    return false;
  }
};

template <>
struct convert<facebook::velox::substrait::SubstraitExtension> {
  static bool decode(
      const Node& node,
      facebook::velox::substrait::SubstraitExtension& extension) {
    auto& scalarFunctions = node["scalar_functions"];
    auto& aggregateFunctions = node["aggregate_functions"];
    const bool scalarFunctionsExists =
        scalarFunctions && scalarFunctions.IsSequence();
    const bool aggregateFunctionsExists =
        aggregateFunctions && aggregateFunctions.IsSequence();
    if (!scalarFunctionsExists && !aggregateFunctionsExists) {
      return false;
    }

    if (scalarFunctionsExists) {
      for (auto& scalarFunctionNode : scalarFunctions) {
        const auto& scalarFunction =
            scalarFunctionNode.as<SubstraitScalarFunction>();
        for (auto& scalaFunctionVariant : scalarFunction.impls) {
          extension.scalarFunctionVariants.emplace_back(scalaFunctionVariant);
        }
      }
    }

    if (aggregateFunctionsExists) {
      for (auto& aggregateFunctionNode : aggregateFunctions) {
        const auto& aggregateFunction =
            aggregateFunctionNode.as<SubstraitAggregateFunction>();
        for (auto& aggregateFunctionVariant : aggregateFunction.impls) {
          extension.aggregateFunctionVariants.emplace_back(
              aggregateFunctionVariant);
        }
      }
    }

    auto& types = node["types"];
    if (types && types.IsSequence()) {
      for (auto& type : types) {
        auto typeAnchor = type.as<SubstraitTypeAnchor>();
        extension.types.emplace_back(
            std::make_shared<SubstraitTypeAnchor>(typeAnchor));
      }
    }

    return true;
  }
};

} // namespace YAML

namespace facebook::velox::substrait {

namespace {

std::string getSubstraitExtensionAbsolutePath() {
  const std::string absolute_path = __FILE__;
  auto const pos = absolute_path.find_last_of('/');
  return absolute_path.substr(0, pos) + "/extensions/";
}

} // namespace

std::shared_ptr<SubstraitExtension> SubstraitExtension::loadExtension() {
  std::vector<std::string> extensionFiles = {
      "functions_aggregate_approx.yaml",
      "functions_aggregate_generic.yaml",
      "functions_arithmetic.yaml",
      "functions_arithmetic_decimal.yaml",
      "functions_boolean.yaml",
      "functions_comparison.yaml",
      "functions_datetime.yaml",
      "functions_logarithmic.yaml",
      "functions_rounding.yaml",
      "functions_string.yaml",
      "functions_set.yaml",
      "unknown.yaml",
  };
  const auto& extensionRootPath = getSubstraitExtensionAbsolutePath();
  return loadExtension(extensionRootPath, extensionFiles);
}

std::shared_ptr<SubstraitExtension> SubstraitExtension::loadExtension(
    const std::string& basePath,
    const std::vector<std::string>& extensionFiles) {
  std::vector<std::string> yamlExtensionFiles;
  yamlExtensionFiles.reserve(extensionFiles.size());
  for (auto& extensionFile : extensionFiles) {
    auto const pos = basePath.find_last_of('/');
    const auto& extensionUri = basePath.substr(0, pos) + "/" + extensionFile;
    yamlExtensionFiles.emplace_back(extensionUri);
  }
  return loadExtension(yamlExtensionFiles);
}

std::shared_ptr<SubstraitExtension> SubstraitExtension::loadExtension(
    const std::vector<std::string>& yamlExtensionFiles) {
  SubstraitExtension mergedExtension;
  for (const auto& extensionUri : yamlExtensionFiles) {
    const auto& substraitExtension =
        YAML::LoadFile(extensionUri).as<SubstraitExtension>();

    for (auto& scalarFunctionVariant :
         substraitExtension.scalarFunctionVariants) {
      scalarFunctionVariant->uri = extensionUri;
      mergedExtension.scalarFunctionVariants.emplace_back(
          scalarFunctionVariant);
    }

    for (auto& aggregateFunctionVariant :
         substraitExtension.aggregateFunctionVariants) {
      aggregateFunctionVariant->uri = extensionUri;
      mergedExtension.aggregateFunctionVariants.emplace_back(
          aggregateFunctionVariant);
    }

    for (auto& type : substraitExtension.types) {
      type->uri = extensionUri;
      mergedExtension.types.emplace_back(type);
    }
  }
  return std::make_shared<SubstraitExtension>(mergedExtension);
}

std::optional<SubstraitFunctionVariantPtr>
SubstraitExtension::lookupScalarFunction(const std::string& signature) const {
  for (const auto& scalarFunctionVariant : scalarFunctionVariants) {
    if (scalarFunctionVariant->signature() == signature) {
      return std::make_optional(scalarFunctionVariant);
    }
  }
  return std::nullopt;
}

std::optional<SubstraitFunctionVariantPtr>
SubstraitExtension::lookupAggregateFunction(
    const std::string& signature) const {
  for (const auto& aggregateFunctionVariant : aggregateFunctionVariants) {
    if (aggregateFunctionVariant->signature() == signature) {
      return std::make_optional(aggregateFunctionVariant);
    }
  }
  return std::nullopt;
}

std::optional<SubstraitFunctionVariantPtr> SubstraitExtension::lookupFunction(
    const std::string& signature) const {
  const auto& function = this->lookupScalarFunction(signature);
  if (!function.has_value()) {
    return this->lookupAggregateFunction(signature);
  }
  return function;
}

std::optional<SubstraitFunctionVariantPtr> SubstraitExtension::lookupFunction(
    const SubstraitFunctionMappingsPtr& functionMappings,
    const std::string& signature) const {
  const auto& functionSignature =
      SubstraitFunctionSignature::signature(signature, functionMappings);
  return this->lookupFunction(functionSignature);
}

} // namespace facebook::velox::substrait
