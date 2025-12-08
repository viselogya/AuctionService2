#include "auction/repository/lot_repository.h"

#include <stdexcept>
#include <string>
#include <vector>

namespace {

constexpr const char* kSelectColumns =
    "id, name, description, start_price, current_price, owner_id, created_at, auction_end_date";

}  // namespace

namespace auction::repository {

LotRepository::LotRepository(core::Database& database) : database_(database) {}

void LotRepository::ensureSchema() {
  database_.query(R"(
    CREATE TABLE IF NOT EXISTS lots (
      id SERIAL PRIMARY KEY,
      name VARCHAR(255) NOT NULL,
      description TEXT,
      start_price NUMERIC(12, 2) NOT NULL,
      current_price NUMERIC(12, 2),
      owner_id VARCHAR(255),
      created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
      auction_end_date TIMESTAMPTZ
    )
  )");

  database_.query("CREATE INDEX IF NOT EXISTS idx_lots_owner_id ON lots(owner_id)");
  database_.query("CREATE INDEX IF NOT EXISTS idx_lots_auction_end_date ON lots(auction_end_date)");
}

void LotRepository::prepareStatements() {
  // Если база данных переподключалась, нужно пересоздать prepared statements
  if (database_.checkAndClearReconnectFlag()) {
    statementsPrepared_ = false;
  }
  
  if (statementsPrepared_) {
    return;
  }

  database_.prepare("lot_select_all", std::string{"SELECT "} + kSelectColumns + " FROM lots ORDER BY created_at DESC");
  database_.prepare("lot_select_by_id", std::string{"SELECT "} + kSelectColumns + " FROM lots WHERE id = $1");
  database_.prepare("lot_insert",
                    "INSERT INTO lots (name, description, start_price, current_price, owner_id, auction_end_date) "
                    "VALUES ($1, $2, $3, $4, $5, $6) RETURNING " +
                        std::string{kSelectColumns});
  database_.prepare("lot_update",
                    "UPDATE lots SET name=$2, description=$3, start_price=$4, current_price=$5, owner_id=$6, "
                    "auction_end_date=$7 WHERE id=$1 RETURNING " +
                        std::string{kSelectColumns});
  database_.prepare("lot_delete", "DELETE FROM lots WHERE id=$1");
  database_.prepare("lot_update_bid",
                    "UPDATE lots SET current_price=$2 WHERE id=$1 RETURNING " + std::string{kSelectColumns});

  statementsPrepared_ = true;
}

model::Lot LotRepository::mapLot(PGresult* result, int row) {
  model::Lot lot;
  lot.id = std::stoi(PQgetvalue(result, row, 0));
  lot.name = PQgetvalue(result, row, 1);

  if (!PQgetisnull(result, row, 2)) {
    lot.description = PQgetvalue(result, row, 2);
  }

  lot.start_price = std::stod(PQgetvalue(result, row, 3));

  if (!PQgetisnull(result, row, 4)) {
    lot.current_price = std::stod(PQgetvalue(result, row, 4));
  }

  if (!PQgetisnull(result, row, 5)) {
    lot.owner_id = PQgetvalue(result, row, 5);
  }

  if (!PQgetisnull(result, row, 6)) {
    lot.created_at = PQgetvalue(result, row, 6);
  }

  if (!PQgetisnull(result, row, 7)) {
    lot.auction_end_date = PQgetvalue(result, row, 7);
  }

  return lot;
}

std::vector<model::Lot> LotRepository::list() {
  prepareStatements();
  auto result = database_.executePrepared("lot_select_all");

  std::vector<model::Lot> lots;
  int rows = PQntuples(result.get());
  lots.reserve(rows);

  for (int i = 0; i < rows; ++i) {
    lots.push_back(mapLot(result.get(), i));
  }

  return lots;
}

std::optional<model::Lot> LotRepository::findById(int id) {
  prepareStatements();

  auto result = database_.executePrepared("lot_select_by_id", {std::to_string(id)});
  if (PQntuples(result.get()) == 0) {
    return std::nullopt;
  }

  return mapLot(result.get(), 0);
}

model::Lot LotRepository::create(const model::Lot& lot) {
  prepareStatements();

  std::vector<std::optional<std::string>> params = {
      lot.name,
      lot.description,
      std::to_string(lot.start_price),
      lot.current_price ? std::optional<std::string>{std::to_string(*lot.current_price)} : std::nullopt,
      lot.owner_id,
      lot.auction_end_date,
  };

  auto result = database_.executePrepared("lot_insert", params);
  if (PQntuples(result.get()) == 0) {
    throw std::runtime_error("Failed to insert lot");
  }

  return mapLot(result.get(), 0);
}

std::optional<model::Lot> LotRepository::update(int id, const model::Lot& lot) {
  prepareStatements();

  std::vector<std::optional<std::string>> params = {
      std::to_string(id),
      lot.name,
      lot.description,
      std::to_string(lot.start_price),
      lot.current_price ? std::optional<std::string>{std::to_string(*lot.current_price)} : std::nullopt,
      lot.owner_id,
      lot.auction_end_date,
  };

  auto result = database_.executePrepared("lot_update", params);
  if (PQntuples(result.get()) == 0) {
    return std::nullopt;
  }

  return mapLot(result.get(), 0);
}

bool LotRepository::remove(int id) {
  prepareStatements();
  auto result = database_.executePrepared("lot_delete", {std::to_string(id)});
  const char* affected = PQcmdTuples(result.get());
  if (!affected) {
    return false;
  }
  return std::stoi(affected) > 0;
}

std::optional<model::Lot> LotRepository::updateCurrentPrice(int id, double bidAmount) {
  prepareStatements();
  auto result = database_.executePrepared("lot_update_bid", {std::to_string(id), std::to_string(bidAmount)});
  if (PQntuples(result.get()) == 0) {
    return std::nullopt;
  }

  return mapLot(result.get(), 0);
}

}  // namespace auction::repository

