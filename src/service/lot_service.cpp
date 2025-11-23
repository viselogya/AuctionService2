#include "auction/service/lot_service.h"

#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace {

std::optional<std::chrono::system_clock::time_point> parseTimestamp(const std::optional<std::string>& value) {
  if (!value || value->empty()) {
    return std::nullopt;
  }

  std::string normalized = *value;
  std::replace(normalized.begin(), normalized.end(), 'T', ' ');

  std::istringstream stream(normalized);
  std::chrono::sys_time<std::chrono::seconds> result;
  stream >> std::chrono::parse("%Y-%m-%d %H:%M:%S%z", result);

  if (stream.fail()) {
    stream.clear();
    stream.seekg(0);
    stream >> std::chrono::parse("%Y-%m-%d %H:%M:%S", result);
  }

  if (stream.fail()) {
    return std::nullopt;
  }

  return result;
}

}  // namespace

namespace auction::service {

LotService::LotService(repository::LotRepository& repository) : repository_(repository) {
  repository_.ensureSchema();
}

std::vector<model::Lot> LotService::listLots() {
  return repository_.list();
}

std::optional<model::Lot> LotService::getLot(int id) {
  if (id <= 0) {
    throw std::invalid_argument("Invalid lot id");
  }

  return repository_.findById(id);
}

model::Lot LotService::createLot(const model::Lot& lot) {
  if (lot.name.empty()) {
    throw std::invalid_argument("Lot name is required");
  }
  if (lot.start_price <= 0) {
    throw std::invalid_argument("start_price must be positive");
  }

  return repository_.create(lot);
}

std::optional<model::Lot> LotService::updateLot(int id, const model::Lot& lot) {
  if (id <= 0) {
    throw std::invalid_argument("Invalid lot id");
  }

  return repository_.update(id, lot);
}

bool LotService::deleteLot(int id) {
  if (id <= 0) {
    throw std::invalid_argument("Invalid lot id");
  }

  return repository_.remove(id);
}

model::Lot LotService::placeBid(int id, double bidAmount) {
  if (id <= 0) {
    throw std::invalid_argument("Invalid lot id");
  }
  if (bidAmount <= 0) {
    throw std::invalid_argument("Bid amount must be positive");
  }

  auto lotOpt = repository_.findById(id);
  if (!lotOpt.has_value()) {
    throw std::runtime_error("Lot not found");
  }

  auto lot = lotOpt.value();

  const double currentPrice = lot.current_price.value_or(lot.start_price);
  if (bidAmount <= lot.start_price || bidAmount <= currentPrice) {
    throw std::runtime_error("Bid must be greater than current and starting price");
  }

  if (auto auctionEnd = parseTimestamp(lot.auction_end_date)) {
    if (std::chrono::system_clock::now() >= *auctionEnd) {
      throw std::runtime_error("Auction already ended");
    }
  }

  auto updated = repository_.updateCurrentPrice(id, bidAmount);
  if (!updated.has_value()) {
    throw std::runtime_error("Failed to place bid");
  }

  return updated.value();
}

}  // namespace auction::service

