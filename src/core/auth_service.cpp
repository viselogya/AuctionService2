#include "auction/core/auth_service.h"

#include <algorithm>
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
  // Хардкод, т.к. переменные окружения не всегда применяются в Railway
  return "https://payment-service-15044579133.europe-central2.run.app";
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

  std::cerr << "=== Token Verification ===" << std::endl;
  std::cerr << "URL: " << verifyUrl_ << std::endl;
  std::cerr << "Token length: " << token.length() << ", first 10 chars: " << token.substr(0, std::min(size_t(10), token.length())) << "..." << std::endl;
  std::cerr << "ServiceName: " << serviceName_ << ", MethodName: " << methodName << std::endl;
  
  const nlohmann::json payload = {{"token", token}, {"serviceName", serviceName_}, {"methodName", methodName}};
  std::cerr << "Request payload: " << payload.dump() << std::endl;
  
  HttpResponse response;
  try {
    response = httpClient_.postJson(verifyUrl_, payload);
  } catch (const std::exception& ex) {
    std::cerr << "Payment Service request failed: " << ex.what() << std::endl;
    throw;
  }
  
  std::cerr << "Payment Service response status: " << response.status << std::endl;
  std::cerr << "Payment Service response body: " << response.body << std::endl;

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

