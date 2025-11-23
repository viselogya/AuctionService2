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

  std::string input = *value;
  std::replace(input.begin(), input.end(), 'T', ' ');

  bool hasOffset = false;
  int offsetMinutes = 0;

  // Handle trailing 'Z'
  if (!input.empty() && (input.back() == 'Z' || input.back() == 'z')) {
    hasOffset = true;
    input.pop_back();
  }

  const auto spacePos = input.find(' ');
  if (spacePos != std::string::npos) {
    std::size_t offsetPos = std::string::npos;
    for (std::size_t i = spacePos + 1; i < input.size(); ++i) {
      if (input[i] == '+' || input[i] == '-') {
        offsetPos = i;
        break;
      }
    }

    if (offsetPos != std::string::npos) {
      hasOffset = true;
      std::string offsetStr = input.substr(offsetPos);
      input = input.substr(0, offsetPos);

      try {
        if (offsetStr.size() >= 3) {
          const int sign = offsetStr[0] == '-' ? -1 : 1;
          std::string hoursPart;
          std::string minutesPart = "0";

          auto colonPos = offsetStr.find(':');
          if (colonPos != std::string::npos) {
            hoursPart = offsetStr.substr(1, colonPos - 1);
            minutesPart = offsetStr.substr(colonPos + 1);
          } else {
            hoursPart = offsetStr.substr(1, 2);
            if (offsetStr.size() >= 5) {
              minutesPart = offsetStr.substr(3, 2);
            }
          }

          offsetMinutes = sign * (std::stoi(hoursPart) * 60 + std::stoi(minutesPart));
        }
      } catch (const std::exception&) {
        return std::nullopt;
      }
    }
  }

  std::tm tm = {};
  std::istringstream stream(input);

  if (!(stream >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S"))) {
    stream.clear();
    stream.str(input);
    if (!(stream >> std::get_time(&tm, "%Y-%m-%d %H:%M"))) {
      stream.clear();
      stream.str(input);
      if (!(stream >> std::get_time(&tm, "%Y-%m-%d"))) {
        return std::nullopt;
      }
    }
  }

  using namespace std::chrono;

  const auto year = std::chrono::year{tm.tm_year + 1900};
  const auto month = std::chrono::month{static_cast<unsigned>(tm.tm_mon + 1)};
  const auto day = std::chrono::day{static_cast<unsigned>(tm.tm_mday)};

  if (!year.ok() || !month.ok() || !day.ok()) {
    return std::nullopt;
  }

  sys_days date{year / month / day};
  auto timePoint = date + hours{tm.tm_hour} + minutes{tm.tm_min} + seconds{tm.tm_sec};

  if (hasOffset && offsetMinutes != 0) {
    timePoint -= minutes{offsetMinutes};
  }

  return timePoint;
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

