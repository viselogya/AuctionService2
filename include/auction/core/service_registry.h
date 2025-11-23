#pragma once

#include <string>
#include <vector>

#include "auction/core/http_client.h"

namespace auction::core {

struct ApiMethod {
  std::string name;
  std::string path;
  std::string description;
};

class ServiceRegistry {
 public:
  ServiceRegistry();

  void registerMethods(const std::vector<ApiMethod>& methods) const;

 private:
  std::string registryUrl_;
  HttpClient httpClient_;

  static std::string resolveRegistryUrl();
};

}  // namespace auction::core

