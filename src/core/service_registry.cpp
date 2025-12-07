#include "auction/core/service_registry.h"

#include <cstdlib>
#include <stdexcept>
#include <string_view>
#include <iostream>

namespace auction::core {

namespace {

std::string normalizeServiceEndpoint(std::string baseUrl) {
  if (baseUrl.empty()) {
    return "http://localhost:9000/service";
  }

  // Trim trailing slash to avoid double slashes
  while (!baseUrl.empty() && baseUrl.back() == '/') {
    baseUrl.pop_back();
  }

  constexpr std::string_view suffix = "/service";
  if (baseUrl.size() >= suffix.size()) {
    const auto pos = baseUrl.rfind(suffix);
    if (pos != std::string::npos && pos == baseUrl.size() - suffix.size()) {
      return baseUrl;  // already contains /service
    }
  }

  return baseUrl + std::string{suffix};
}

}  // namespace

std::string ServiceRegistry::resolveRegistryUrl() {
  if (const char* value = std::getenv("SERVICE_REGISTRY_URL"); value != nullptr && *value != '\0') {
    return normalizeServiceEndpoint(value);
  }
  return "http://localhost:9000/service";
}

std::string ServiceRegistry::resolveServiceName() {
  if (const char* value = std::getenv("SERVICE_NAME"); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  return "AuctionService";
}

ServiceRegistry::ServiceRegistry() : registryUrl_(resolveRegistryUrl()), serviceName_(resolveServiceName()) {}

void ServiceRegistry::registerMethods(const std::vector<ApiMethod>& methods) const {
  nlohmann::json payload;
  payload["serviceName"] = serviceName_;

  nlohmann::json methodsJson = nlohmann::json::array();
  for (const auto& method : methods) {
    nlohmann::json methodJson;
    methodJson["methodName"] = method.methodName;
    methodJson["price"] = method.price;
    methodJson["isPrivate"] = method.isPrivate;

    nlohmann::json argumentsJson = nlohmann::json::array();
    for (const auto& arg : method.arguments) {
      argumentsJson.push_back({{"argumentNumber", arg.argumentNumber},
                               {"argumentName", arg.argumentName},
                               {"argumentType", arg.argumentType},
                               {"isRequired", arg.isRequired}});
    }

    methodJson["arguments"] = std::move(argumentsJson);
    methodsJson.push_back(std::move(methodJson));
  }
  payload["methods"] = std::move(methodsJson);

  std::cout << "Registering service at: " << registryUrl_ << std::endl;
  std::cout << "Registry payload: " << payload.dump(2) << std::endl;

  const auto response = httpClient_.postJson(registryUrl_, payload);
  if (response.status < 200 || response.status >= 300) {
    throw std::runtime_error("Service registry call failed with status " + std::to_string(response.status));
  }
}

}  // namespace auction::core

