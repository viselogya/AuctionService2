#include "auction/api/routes.h"

#include <string>
#include <vector>

#include <nlohmann/json.hpp>

#include "auction/model/lot.h"

namespace auction::api {

namespace {

void respondJson(httplib::Response& res, int status, const nlohmann::json& body) {
  res.status = status;
  if (status == 204) {
    res.set_content("", "application/json");
  } else {
    res.set_content(body.dump(), "application/json");
  }
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

core::ApiArgument makeArgument(int number, std::string name, std::string type, bool required) {
  return core::ApiArgument{
      .argumentNumber = number,
      .argumentName = std::move(name),
      .argumentType = std::move(type),
      .isRequired = required,
  };
}

void applyLotPatch(model::Lot& lot, const nlohmann::json& body) {
  if (body.contains("name") && !body.at("name").is_null()) {
    lot.name = body.at("name").get<std::string>();
  }
  if (body.contains("description")) {
    lot.description = body.at("description").is_null() ? std::optional<std::string>{}
                                                      : std::optional<std::string>{body.at("description").get<std::string>()};
  }
  if (body.contains("owner_id")) {
    lot.owner_id = body.at("owner_id").is_null() ? std::optional<std::string>{}
                                                 : std::optional<std::string>{body.at("owner_id").get<std::string>()};
  }
  if (body.contains("auction_end_date")) {
    lot.auction_end_date =
        body.at("auction_end_date").is_null() ? std::optional<std::string>{}
                                              : std::optional<std::string>{body.at("auction_end_date").get<std::string>()};
  }
  if (body.contains("start_price") && !body.at("start_price").is_null()) {
    lot.start_price = body.at("start_price").get<double>();
  }
  if (body.contains("current_price")) {
    lot.current_price = body.at("current_price").is_null() ? std::optional<double>{}
                                                           : std::optional<double>{body.at("current_price").get<double>()};
  }
}

std::vector<core::ApiMethod> registerRoutes(httplib::Server& server, service::LotService& lotService,
                                            core::AuthService& authService) {
  std::vector<core::ApiMethod> methods = {
      {.methodName = "GET /lots", .price = 0.0, .isPrivate = false, .arguments = {}},
      {.methodName = "GET /lots/{id}",
       .price = 0.0,
       .isPrivate = false,
       .arguments = {makeArgument(1, "id", "int", true)}},
      {.methodName = "POST /lots",
       .price = 0.0,
       .isPrivate = false,
       .arguments =
           {
               makeArgument(1, "name", "string", true),
               makeArgument(2, "description", "string", false),
               makeArgument(3, "start_price", "decimal", true),
               makeArgument(4, "owner_id", "string", true),
               makeArgument(5, "auction_end_date", "timestamp", false),
           }},
      {.methodName = "PUT /lots/{id}",
       .price = 0.0,
       .isPrivate = false,
       .arguments =
           {
               makeArgument(1, "id", "int", true),
               makeArgument(2, "name", "string", false),
               makeArgument(3, "description", "string", false),
               makeArgument(4, "owner_id", "string", false),
               makeArgument(5, "auction_end_date", "timestamp", false),
           }},
      {.methodName = "DELETE /lots/{id}",
       .price = 0.0,
       .isPrivate = false,
       .arguments = {makeArgument(1, "id", "int", true)}},
      {.methodName = "POST /lots/{id}/bid",
       .price = 0.0,
       .isPrivate = false,
       .arguments = {makeArgument(1, "id", "int", true), makeArgument(2, "amount", "decimal", true)}},
      {.methodName = "GET /health", .price = 0.0, .isPrivate = false, .arguments = {}},
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

  server.Put(R"(/lots/(\d+))",
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

                 const auto body = nlohmann::json::parse(req.body);
                 applyLotPatch(*lot, body);
                 auto updated = lotService.updateLot(id, *lot);
                 if (!updated.has_value()) {
                   respondJson(res, 500, {{"error", "Failed to update lot"}});
                   return;
                 }
                 respondJson(res, 200, updated->toJson());
               } catch (const nlohmann::json::exception&) {
                 respondJson(res, 400, {{"error", "Invalid JSON payload"}});
               } catch (const std::invalid_argument&) {
                 respondJson(res, 400, {{"error", "Invalid id"}});
               } catch (const std::exception& ex) {
                 respondJson(res, 400, {{"error", ex.what()}});
               }
             });

  server.Delete(R"(/lots/(\d+))",
                [&lotService, &authService](const httplib::Request& req, httplib::Response& res) {
                  if (!requireAuth(req, res, authService)) {
                    return;
                  }

                  try {
                    const int id = std::stoi(req.matches[1]);
                    if (!lotService.deleteLot(id)) {
                      respondJson(res, 404, {{"error", "Lot not found"}});
                      return;
                    }
                    respondJson(res, 204, nlohmann::json::object());
                  } catch (const std::invalid_argument&) {
                    respondJson(res, 400, {{"error", "Invalid id"}});
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

