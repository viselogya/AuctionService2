#pragma once

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <libpq-fe.h>

namespace auction::core {

class Database {
 public:
  using ResultPtr = std::unique_ptr<PGresult, decltype(&PQclear)>;

  Database();
  ~Database();

  Database(const Database&) = delete;
  Database& operator=(const Database&) = delete;
  Database(Database&&) = delete;
  Database& operator=(Database&&) = delete;

  ResultPtr query(const std::string& sql, const std::vector<std::optional<std::string>>& params = {});
  void prepare(const std::string& name, const std::string& sql);
  ResultPtr executePrepared(const std::string& name, const std::vector<std::optional<std::string>>& params = {});
  
  // Проверить, был ли reconnect (prepared statements нужно пересоздать)
  bool checkAndClearReconnectFlag();

 private:
  PGconn* connection_{nullptr};
  std::mutex mutex_;
  std::string connectionString_;
  bool needReconnect_{false};

  static ResultPtr makeResult(PGresult* result);
  static std::string buildConnectionString();
  void ensureConnected();
  void reconnect();
};

}  // namespace auction::core

