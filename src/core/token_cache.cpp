#include "auction/core/token_cache.h"

namespace auction::core {

TokenCache::TokenCache(std::chrono::seconds ttl) : ttl_(ttl) {}

std::optional<bool> TokenCache::get(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  purgeExpiredLocked();

  auto it = cache_.find(token);
  if (it == cache_.end()) {
    return std::nullopt;
  }

  return it->second.valid;
}

void TokenCache::put(const std::string& token, bool isValid) {
  std::lock_guard<std::mutex> lock(mutex_);
  Entry entry{isValid, std::chrono::steady_clock::now() + ttl_};
  cache_[token] = entry;
}

void TokenCache::clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  cache_.clear();
}

void TokenCache::purgeExpiredLocked() {
  const auto now = std::chrono::steady_clock::now();
  for (auto it = cache_.begin(); it != cache_.end();) {
    if (it->second.expires_at <= now) {
      it = cache_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace auction::core

