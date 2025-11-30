#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "auction/core/http_client.h"
#include "auction/core/token_cache.h"

namespace auction::core {

class AuthService {
 public:
  explicit AuthService(TokenCache& cache);

  bool verifyToken(const std::string& token, const std::string& methodName);

 private:
  std::string verifyUrl_;
  std::string serviceName_;
  TokenCache& cache_;
  HttpClient httpClient_;

  static std::string resolveBaseUrl();
  static std::string resolveServiceName();
};

}  // namespace auction::core

