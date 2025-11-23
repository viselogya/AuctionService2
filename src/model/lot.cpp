#include "auction/model/lot.h"

#include <stdexcept>

namespace auction::model {

nlohmann::json Lot::toJson() const {
  nlohmann::json json = {
      {"id", id},
      {"name", name},
      {"start_price", start_price},
      {"created_at", created_at},
  };

  if (description.has_value()) {
    json["description"] = *description;
  } else {
    json["description"] = nullptr;
  }

  if (current_price.has_value()) {
    json["current_price"] = *current_price;
  } else {
    json["current_price"] = nullptr;
  }

  if (owner_id.has_value()) {
    json["owner_id"] = *owner_id;
  } else {
    json["owner_id"] = nullptr;
  }

  if (auction_end_date.has_value()) {
    json["auction_end_date"] = *auction_end_date;
  } else {
    json["auction_end_date"] = nullptr;
  }

  return json;
}

Lot lotFromJson(const nlohmann::json& json) {
  Lot lot;

  if (!json.contains("name") || !json.contains("start_price")) {
    throw std::invalid_argument("Missing required fields: name or start_price");
  }

  lot.name = json.at("name").get<std::string>();
  lot.start_price = json.at("start_price").get<double>();

  if (json.contains("description") && !json.at("description").is_null()) {
    lot.description = json.at("description").get<std::string>();
  }

  if (json.contains("owner_id") && !json.at("owner_id").is_null()) {
    lot.owner_id = json.at("owner_id").get<std::string>();
  }

  if (json.contains("auction_end_date") && !json.at("auction_end_date").is_null()) {
    lot.auction_end_date = json.at("auction_end_date").get<std::string>();
  }

  if (json.contains("current_price") && !json.at("current_price").is_null()) {
    lot.current_price = json.at("current_price").get<double>();
  }

  if (json.contains("created_at") && !json.at("created_at").is_null()) {
    lot.created_at = json.at("created_at").get<std::string>();
  }

  return lot;
}

}  // namespace auction::model

