#include "auction/core/database.h"

#include <cstdlib>
#include <stdexcept>
#include <string>
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

  std::string connection =
      "host=" + host + " dbname=" + database + " user=" + user + " password=" + password + " port=" + port;
  return connection;
}

Database::Database() {
  const std::string connectionInfo = buildConnectionString();
  connection_ = PQconnectdb(connectionInfo.c_str());
  if (!connection_ || PQstatus(connection_) != CONNECTION_OK) {
    throw std::runtime_error(std::string{"Failed to connect to database: "} + PQerrorMessage(connection_));
  }
}

Database::~Database() {
  if (connection_) {
    PQfinish(connection_);
    connection_ = nullptr;
  }
}

void Database::ensureConnected() {
  if (!connection_) {
    throw std::runtime_error("Database connection is not initialized");
  }

  if (PQstatus(connection_) != CONNECTION_OK) {
    throw std::runtime_error(std::string{"Database connection lost: "} + PQerrorMessage(connection_));
  }
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

