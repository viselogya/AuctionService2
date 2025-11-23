#pragma once

#include <optional>
#include <vector>

#include "auction/model/lot.h"
#include "auction/repository/lot_repository.h"

namespace auction::service {

class LotService {
 public:
  explicit LotService(repository::LotRepository& repository);

  std::vector<model::Lot> listLots();
  std::optional<model::Lot> getLot(int id);
  model::Lot createLot(const model::Lot& lot);
  std::optional<model::Lot> updateLot(int id, const model::Lot& lot);
  bool deleteLot(int id);
  model::Lot placeBid(int id, double bidAmount);

 private:
  repository::LotRepository& repository_;
};

}  // namespace auction::service

