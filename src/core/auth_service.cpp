#include "auction/core/auth_service.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace auction::core {

namespace {

std::string joinUrl(const std::string& base, const std::string& path) {
  if (base.empty()) {
    return path;
  }

  if (base.back() == '/' && path.front() == '/') {
    return base + path.substr(1);
  }

  if (base.back() != '/' && path.front() != '/') {
    return base + "/" + path;
  }

  return base + path;
}

}  // namespace

std::string AuthService::resolveBaseUrl() {
  if (const char* value = std::getenv("PAYMENT_SERVICE_URL"); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  return "http://localhost:8081";
}

std::string AuthService::resolveServiceName() {
  if (const char* value = std::getenv("SERVICE_NAME"); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  return "AuctionService";
}

AuthService::AuthService(TokenCache& cache)
    : verifyUrl_(joinUrl(resolveBaseUrl(), "/token/check")), serviceName_(resolveServiceName()), cache_(cache) {}

bool AuthService::verifyToken(const std::string& token, const std::string& methodName) {
  if (token.empty()) {
    return false;
  }

  const std::string cacheKey = token + "::" + methodName;
  if (auto cached = cache_.get(cacheKey)) {
    return *cached;
  }

  const nlohmann::json payload = {{"token", token}, {"serviceName", serviceName_}, {"methodName", methodName}};
  
  HttpResponse response;
  try {
    response = httpClient_.postJson(verifyUrl_, payload);
  } catch (const std::exception& ex) {
    std::cerr << "Payment Service request failed: " << ex.what() << std::endl;
    throw;
  }

  if (response.status == 401 || response.status == 403) {
    cache_.put(cacheKey, false);
    return false;
  }

  if (response.status != 200) {
    throw std::runtime_error("Token verification service returned status " + std::to_string(response.status));
  }

  try {
    const auto json = nlohmann::json::parse(response.body);
    const bool allowed = json.value("allowed", false);
    cache_.put(cacheKey, allowed);
    return allowed;
  } catch (const nlohmann::json::exception& ex) {
    throw std::runtime_error(std::string{"Failed to parse token verification response: "} + ex.what());
  }
}

}  // namespace auction::core

