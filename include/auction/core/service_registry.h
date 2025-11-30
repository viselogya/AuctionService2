#pragma once

#include <string>
#include <vector>

#include "auction/core/http_client.h"

namespace auction::core {

struct ApiArgument {
  int argumentNumber;
  std::string argumentName;
  std::string argumentType;
  bool isRequired;
};

struct ApiMethod {
  std::string methodName;
  double price;
  bool isPrivate;
  std::vector<ApiArgument> arguments;
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

