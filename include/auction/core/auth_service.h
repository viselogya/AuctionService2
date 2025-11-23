#pragma once

#include <string>

#include <nlohmann/json.hpp>

#include "auction/core/http_client.h"
#include "auction/core/token_cache.h"

namespace auction::core {

class AuthService {
 public:
  explicit AuthService(TokenCache& cache);

  bool verifyToken(const std::string& token);

 private:
  std::string verifyUrl_;
  TokenCache& cache_;
  HttpClient httpClient_;

  static std::string resolveBaseUrl();
};

}  // namespace auction::core

