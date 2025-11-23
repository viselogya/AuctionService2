#pragma once

#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace auction::model {

struct Lot {
  int id{};
  std::string name;
  std::optional<std::string> description;
  double start_price{};
  std::optional<double> current_price;
  std::optional<std::string> owner_id;
  std::string created_at;
  std::optional<std::string> auction_end_date;

  [[nodiscard]] nlohmann::json toJson() const;
};

Lot lotFromJson(const nlohmann::json& json);

}  // namespace auction::model

