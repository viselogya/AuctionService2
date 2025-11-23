#pragma once

#include <chrono>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>

namespace auction::core {

class TokenCache {
 public:
  explicit TokenCache(std::chrono::seconds ttl = std::chrono::seconds{60});

  std::optional<bool> get(const std::string& token);
  void put(const std::string& token, bool isValid);
  void clear();

 private:
  struct Entry {
    bool valid;
    std::chrono::steady_clock::time_point expires_at;
  };

  std::chrono::seconds ttl_;
  std::unordered_map<std::string, Entry> cache_;
  std::mutex mutex_;

  void purgeExpiredLocked();
};

}  // namespace auction::core

