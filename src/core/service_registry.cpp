#include "auction/core/service_registry.h"

#include <cstdlib>
#include <stdexcept>

namespace auction::core {

std::string ServiceRegistry::resolveRegistryUrl() {
  if (const char* value = std::getenv("SERVICE_REGISTRY_URL"); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  return "http://localhost:9000/register";
}

ServiceRegistry::ServiceRegistry() : registryUrl_(resolveRegistryUrl()) {}

void ServiceRegistry::registerMethods(const std::vector<ApiMethod>& methods) const {
  nlohmann::json payload;
  payload["service_name"] = "auction";

  nlohmann::json methodsJson = nlohmann::json::array();
  for (const auto& method : methods) {
    methodsJson.push_back({{"name", method.name}, {"path", method.path}, {"description", method.description}});
  }
  payload["methods"] = std::move(methodsJson);

  const auto response = httpClient_.postJson(registryUrl_, payload);
  if (response.status < 200 || response.status >= 300) {
    throw std::runtime_error("Service registry call failed with status " + std::to_string(response.status));
  }
}

}  // namespace auction::core

