#pragma once

#include <optional>
#include <vector>

#include "auction/core/database.h"
#include "auction/model/lot.h"

namespace auction::repository {

class LotRepository {
 public:
  explicit LotRepository(core::Database& database);

  void ensureSchema();

  std::vector<model::Lot> list();
  std::optional<model::Lot> findById(int id);
  model::Lot create(const model::Lot& lot);
  std::optional<model::Lot> update(int id, const model::Lot& lot);
  bool remove(int id);
  std::optional<model::Lot> updateCurrentPrice(int id, double bidAmount);

 private:
  core::Database& database_;
  bool statementsPrepared_{false};

  static model::Lot mapLot(PGresult* result, int row);
  void prepareStatements();
};

}  // namespace auction::repository

