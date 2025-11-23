#pragma once

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace auction::core {

struct HttpResponse {
  long status{0};
  std::string body;
};

class HttpClient {
 public:
  HttpResponse postJson(const std::string& url, const nlohmann::json& payload,
                        const std::vector<std::string>& headers = {}) const;
};

}  // namespace auction::core

