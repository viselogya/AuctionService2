#include "auction/core/database.h"

#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

std::string requireEnv(const char* key) {
  if (const char* value = std::getenv(key); value != nullptr && *value != '\0') {
    return std::string{value};
  }
  throw std::runtime_error(std::string{"Missing required environment variable: "} + key);
}

bool isSuccessExec(PGresult* result) {
  ExecStatusType status = PQresultStatus(result);
  return status == PGRES_TUPLES_OK || status == PGRES_COMMAND_OK;
}

}  // namespace

namespace auction::core {

Database::ResultPtr Database::makeResult(PGresult* result) {
  return ResultPtr(result, &PQclear);
}

std::string Database::buildConnectionString() {
  const std::string host = requireEnv("SUPABASE_HOST");
  const std::string database = requireEnv("SUPABASE_DB");
  const std::string user = requireEnv("SUPABASE_USER");
  const std::string password = requireEnv("SUPABASE_PASSWORD");
  const std::string port = requireEnv("SUPABASE_PORT");

  // Добавляем keepalive параметры для обнаружения разрывов соединения
  std::string connection =
      "host=" + host + " dbname=" + database + " user=" + user + " password=" + password + " port=" + port +
      " keepalives=1 keepalives_idle=30 keepalives_interval=10 keepalives_count=3 connect_timeout=10";
  return connection;
}

Database::Database() {
  connectionString_ = buildConnectionString();
  connection_ = PQconnectdb(connectionString_.c_str());
  if (!connection_ || PQstatus(connection_) != CONNECTION_OK) {
    throw std::runtime_error(std::string{"Failed to connect to database: "} + PQerrorMessage(connection_));
  }
  std::cerr << "Database connected successfully" << std::endl;
}

Database::~Database() {
  if (connection_) {
    PQfinish(connection_);
    connection_ = nullptr;
  }
}

void Database::reconnect() {
  std::cerr << "Attempting to reconnect to database..." << std::endl;
  
  if (connection_) {
    PQfinish(connection_);
    connection_ = nullptr;
  }
  
  // Пробуем переподключиться до 3 раз
  for (int attempt = 1; attempt <= 3; ++attempt) {
    std::cerr << "Reconnect attempt " << attempt << "/3" << std::endl;
    
    connection_ = PQconnectdb(connectionString_.c_str());
    if (connection_ && PQstatus(connection_) == CONNECTION_OK) {
      std::cerr << "Database reconnected successfully" << std::endl;
      needReconnect_ = true;  // Сигнал, что prepared statements нужно пересоздать
      return;
    }
    
    std::cerr << "Reconnect failed: " << (connection_ ? PQerrorMessage(connection_) : "null connection") << std::endl;
    
    if (attempt < 3) {
      std::this_thread::sleep_for(std::chrono::milliseconds(500 * attempt));
    }
  }
  
  throw std::runtime_error("Failed to reconnect to database after 3 attempts");
}

bool Database::checkAndClearReconnectFlag() {
  if (needReconnect_) {
    needReconnect_ = false;
    return true;
  }
  return false;
}

void Database::ensureConnected() {
  if (!connection_) {
    reconnect();
    return;
  }

  // Проверяем статус соединения
  if (PQstatus(connection_) != CONNECTION_OK) {
    std::cerr << "Database connection lost, attempting reconnect..." << std::endl;
    reconnect();
    return;
  }
  
  // Дополнительно делаем ping для проверки живости соединения
  PGresult* result = PQexec(connection_, "SELECT 1");
  if (!result || PQresultStatus(result) != PGRES_TUPLES_OK) {
    if (result) PQclear(result);
    std::cerr << "Database ping failed, attempting reconnect..." << std::endl;
    reconnect();
    return;
  }
  PQclear(result);
}

Database::ResultPtr Database::query(const std::string& sql, const std::vector<std::optional<std::string>>& params) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensureConnected();

  std::vector<const char*> values(params.size(), nullptr);
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (params[i].has_value()) {
      values[i] = params[i]->c_str();
    }
  }

  PGresult* rawResult =
      PQexecParams(connection_, sql.c_str(), static_cast<int>(values.size()), nullptr, values.data(), nullptr, nullptr, 0);

  if (!rawResult || !isSuccessExec(rawResult)) {
    std::string error = rawResult ? PQresultErrorMessage(rawResult) : PQerrorMessage(connection_);
    if (rawResult) {
      PQclear(rawResult);
    }
    throw std::runtime_error("Database query failed: " + error);
  }

  return makeResult(rawResult);
}

void Database::prepare(const std::string& name, const std::string& sql) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensureConnected();

  PGresult* rawResult = PQprepare(connection_, name.c_str(), sql.c_str(), 0, nullptr);
  if (!rawResult || PQresultStatus(rawResult) != PGRES_COMMAND_OK) {
    std::string error = rawResult ? PQresultErrorMessage(rawResult) : PQerrorMessage(connection_);
    if (rawResult) {
      PQclear(rawResult);
    }
    throw std::runtime_error("Database prepare failed: " + error);
  }

  PQclear(rawResult);
}

Database::ResultPtr Database::executePrepared(const std::string& name,
                                              const std::vector<std::optional<std::string>>& params) {
  std::lock_guard<std::mutex> lock(mutex_);
  ensureConnected();

  std::vector<const char*> values(params.size(), nullptr);
  for (std::size_t i = 0; i < params.size(); ++i) {
    if (params[i].has_value()) {
      values[i] = params[i]->c_str();
    }
  }

  PGresult* rawResult = PQexecPrepared(connection_, name.c_str(), static_cast<int>(values.size()), values.data(), nullptr,
                                       nullptr, 0);

  if (!rawResult || !isSuccessExec(rawResult)) {
    std::string error = rawResult ? PQresultErrorMessage(rawResult) : PQerrorMessage(connection_);
    if (rawResult) {
      PQclear(rawResult);
    }
    throw std::runtime_error("Database execute prepared failed: " + error);
  }

  return makeResult(rawResult);
}

}  // namespace auction::core

