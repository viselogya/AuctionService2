#include "auction/api/routes.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "auction/model/lot.h"

namespace auction::api {

namespace {

void respondJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

bool requireAuth(const httplib::Request& req, httplib::Response& res, core::AuthService& authService) {
  const auto& authHeader = req.get_header_value("Authorization");
  if (authHeader.empty()) {
    respondJson(res, 401, {{"error", "Missing Authorization header"}});
    return false;
  }

  constexpr std::string_view bearer = "Bearer ";
  if (authHeader.size() <= bearer.size() || authHeader.compare(0, bearer.size(), bearer) != 0) {
    respondJson(res, 401, {{"error", "Invalid Authorization header"}});
    return false;
  }

  const std::string token = authHeader.substr(bearer.size());
  try {
    if (!authService.verifyToken(token)) {
      respondJson(res, 403, {{"error", "Invalid token"}});
      return false;
    }
  } catch (const std::exception& ex) {
    respondJson(res, 502, {{"error", ex.what()}});
    return false;
  }

  return true;
}

}  // namespace

std::vector<core::ApiMethod> registerRoutes(httplib::Server& server, service::LotService& lotService,
                                            core::AuthService& authService) {
  std::vector<core::ApiMethod> methods = {
      {"ListLots", "/lots", "GET - Return all lots"},
      {"GetLot", "/lots/{id}", "GET - Return lot by id"},
      {"CreateLot", "/lots", "POST - Create a new lot"},
      {"PlaceBid", "/lots/{id}/bid", "POST - Place a bid on a lot"},
      {"Health", "/health", "GET - Health check"},
  };

  server.Get("/health", [](const httplib::Request&, httplib::Response& res) {
    respondJson(res, 200, {{"status", "ok"}});
  });

  server.Get("/lots", [&lotService, &authService](const httplib::Request& req, httplib::Response& res) {
    if (!requireAuth(req, res, authService)) {
      return;
    }

    try {
      const auto lots = lotService.listLots();
      nlohmann::json body = nlohmann::json::array();
      for (const auto& lot : lots) {
        body.push_back(lot.toJson());
      }
      respondJson(res, 200, body);
    } catch (const std::exception& ex) {
      respondJson(res, 500, {{"error", ex.what()}});
    }
  });

  server.Get(R"(/lots/(\d+))",
             [&lotService, &authService](const httplib::Request& req, httplib::Response& res) {
               if (!requireAuth(req, res, authService)) {
                 return;
               }

               try {
                 const int id = std::stoi(req.matches[1]);
                 auto lot = lotService.getLot(id);
                 if (!lot.has_value()) {
                   respondJson(res, 404, {{"error", "Lot not found"}});
                   return;
                 }
                 respondJson(res, 200, lot->toJson());
               } catch (const std::invalid_argument&) {
                 respondJson(res, 400, {{"error", "Invalid id"}});
               } catch (const std::exception& ex) {
                 respondJson(res, 500, {{"error", ex.what()}});
               }
             });

  server.Post("/lots", [&lotService, &authService](const httplib::Request& req, httplib::Response& res) {
    if (!requireAuth(req, res, authService)) {
      return;
    }

    try {
      const auto body = nlohmann::json::parse(req.body);
      auto lot = model::lotFromJson(body);
      lot.created_at.clear();
      auto created = lotService.createLot(lot);
      respondJson(res, 201, created.toJson());
    } catch (const nlohmann::json::exception&) {
      respondJson(res, 400, {{"error", "Invalid JSON payload"}});
    } catch (const std::exception& ex) {
      respondJson(res, 400, {{"error", ex.what()}});
    }
  });

  server.Post(R"(/lots/(\d+)/bid)",
              [&lotService, &authService](const httplib::Request& req, httplib::Response& res) {
                if (!requireAuth(req, res, authService)) {
                  return;
                }

                try {
                  const int id = std::stoi(req.matches[1]);
                  const auto body = nlohmann::json::parse(req.body);
                  if (!body.contains("amount")) {
                    respondJson(res, 400, {{"error", "Missing amount"}});
                    return;
                  }
                  const double amount = body.at("amount").get<double>();
                  auto lot = lotService.placeBid(id, amount);
                  respondJson(res, 200, lot.toJson());
                } catch (const nlohmann::json::exception&) {
                  respondJson(res, 400, {{"error", "Invalid JSON payload"}});
                } catch (const std::invalid_argument&) {
                  respondJson(res, 400, {{"error", "Invalid id or amount"}});
                } catch (const std::exception& ex) {
                  respondJson(res, 400, {{"error", ex.what()}});
                }
              });

  return methods;
}

}  // namespace auction::api

