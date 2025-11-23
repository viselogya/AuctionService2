#include "auction/core/auth_service.h"

#include <cstdlib>
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

AuthService::AuthService(TokenCache& cache) : verifyUrl_(joinUrl(resolveBaseUrl(), "/verify")), cache_(cache) {}

bool AuthService::verifyToken(const std::string& token) {
  if (token.empty()) {
    return false;
  }

  if (auto cached = cache_.get(token)) {
    return *cached;
  }

  const nlohmann::json payload = {{"token", token}};
  const auto response = httpClient_.postJson(verifyUrl_, payload);

  if (response.status != 200) {
    throw std::runtime_error("Token verification service returned status " + std::to_string(response.status));
  }

  try {
    const auto json = nlohmann::json::parse(response.body);
    const bool valid = json.value("valid", false);
    cache_.put(token, valid);
    return valid;
  } catch (const nlohmann::json::exception& ex) {
    throw std::runtime_error(std::string{"Failed to parse token verification response: "} + ex.what());
  }
}

}  // namespace auction::core

