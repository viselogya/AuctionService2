#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

#include <httplib.h>
#include <curl/curl.h>

#include "auction/api/routes.h"
#include "auction/core/auth_service.h"
#include "auction/core/database.h"
#include "auction/core/service_registry.h"
#include "auction/core/token_cache.h"
#include "auction/repository/lot_repository.h"
#include "auction/service/lot_service.h"

std::string requireEnvOrDefault(const char* key, const std::string& fallback) {
  if (const char* value = std::getenv(key); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  return fallback;
}

class CurlGlobalGuard {
 public:
  CurlGlobalGuard() {
    if (curl_global_init(CURL_GLOBAL_DEFAULT) != 0) {
      throw std::runtime_error("Failed to initialize CURL");
    }
  }

  CurlGlobalGuard(const CurlGlobalGuard&) = delete;
  CurlGlobalGuard& operator=(const CurlGlobalGuard&) = delete;
  CurlGlobalGuard(CurlGlobalGuard&&) = delete;
  CurlGlobalGuard& operator=(CurlGlobalGuard&&) = delete;

  ~CurlGlobalGuard() { curl_global_cleanup(); }
};

int main() {
  try {
    CurlGlobalGuard curlGuard;

    auction::core::Database database;
    auction::repository::LotRepository repository(database);
    auction::service::LotService lotService(repository);
    auction::core::TokenCache tokenCache;
    auction::core::AuthService authService(tokenCache);

    httplib::Server server;
    auto methods = auction::api::registerRoutes(server, lotService, authService);

    try {
      auction::core::ServiceRegistry registry;
      registry.registerMethods(methods);
      std::cout << "Service registry updated successfully" << std::endl;
    } catch (const std::exception& ex) {
      std::cerr << "Warning: failed to register service in registry: " << ex.what() << std::endl;
    }

    const std::string host = requireEnvOrDefault("SERVER_HOST", "0.0.0.0");
    const int port = std::stoi(requireEnvOrDefault("SERVER_PORT", "8080"));

    std::cout << "Auction service is starting on " << host << ":" << port << std::endl;
    if (!server.listen(host.c_str(), port)) {
      std::cerr << "Failed to start HTTP server" << std::endl;
      return EXIT_FAILURE;
    }
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

