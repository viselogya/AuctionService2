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

void logEnvVar(const char* name) {
  const char* value = std::getenv(name);
  if (value != nullptr && *value != '\0') {
    std::cerr << "ENV " << name << " = " << value << std::endl;
  } else {
    std::cerr << "ENV " << name << " = (not set)" << std::endl;
  }
}

int main() {
  try {
    std::cerr << "=== Environment Variables ===" << std::endl;
    logEnvVar("PAYMENT_SERVICE_URL");
    logEnvVar("SERVICE_REGISTRY_URL");
    logEnvVar("SERVICE_NAME");
    logEnvVar("SERVER_HOST");
    logEnvVar("SERVER_PORT");
    logEnvVar("PORT");
    logEnvVar("SUPABASE_HOST");
    logEnvVar("SUPABASE_PORT");
    logEnvVar("SUPABASE_DB");
    logEnvVar("SUPABASE_USER");
    std::cerr << "==============================" << std::endl;

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
    const std::string portString = requireEnvOrDefault("SERVER_PORT", requireEnvOrDefault("PORT", "8080"));
    const int port = std::stoi(portString);

    std::cout << "Auction service is starting on " << host << ":" << port << std::endl;
    std::cout << "DEBUG: host length=" << host.length() << ", port=" << port << std::endl;
    
    // Try to bind first to get more info
    if (!server.bind_to_port(host.c_str(), port)) {
      std::cerr << "Failed to bind to " << host << ":" << port << std::endl;
      std::cerr << "Trying to bind to 0.0.0.0:" << port << " instead..." << std::endl;
      if (!server.bind_to_port("0.0.0.0", port)) {
        std::cerr << "Failed to bind to 0.0.0.0:" << port << " as well" << std::endl;
        return EXIT_FAILURE;
      }
    }
    
    std::cout << "Successfully bound, starting to listen..." << std::endl;
    if (!server.listen_after_bind()) {
      std::cerr << "Failed to start HTTP server after bind" << std::endl;
      return EXIT_FAILURE;
    }
  } catch (const std::exception& ex) {
    std::cerr << "Fatal error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}

