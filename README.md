# Auction Service

Auction microservice that exposes CRUD and bidding operations for lots, integrates with Supabase/PostgreSQL through `libpq`, registers itself in a service registry, and validates requests against an external payment service.

## Project Layout

```
/include/auction     Public headers
/src                 Implementation (core, repository, service, api)
main.cpp             Application entry point
Dockerfile           Multi-stage build for deployment
CMakeLists.txt       Build configuration
README.md            This document
```

## Environment Variables

| Variable | Description | Required |
|----------|-------------|----------|
| `SUPABASE_HOST` | Supabase/PostgreSQL host | Yes |
| `SUPABASE_DB` | Database name | Yes |
| `SUPABASE_USER` | Database user | Yes |
| `SUPABASE_PASSWORD` | Database password | Yes |
| `SUPABASE_PORT` | Database port | Yes |
| `SERVICE_REGISTRY_URL` | URL for registering methods (`POST`) | Optional (`http://localhost:9000/register`) |
| `PAYMENT_SERVICE_URL` | Payment service base URL (expects `/verify`) | Optional (`http://localhost:8081`) |
| `SERVER_HOST` | HTTP server host binding | Optional (`0.0.0.0`) |
| `SERVER_PORT` | HTTP server port | Optional (`8080`) |

## Build Prerequisites

- C++20-capable compiler (GCC 11+, Clang 13+, MSVC 19.3+)
- CMake 3.18+
- `libpq` development headers
- `libcurl` development headers

### Configure & Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target auction_service
```

### Run Locally

```bash
export SUPABASE_HOST=...
export SUPABASE_DB=...
export SUPABASE_USER=...
export SUPABASE_PASSWORD=...
export SUPABASE_PORT=...
# optionally:
export PAYMENT_SERVICE_URL=https://payments.example.com
export SERVICE_REGISTRY_URL=https://registry.example.com/register
./build/auction_service
```

The server listens on `SERVER_HOST:SERVER_PORT` (default `0.0.0.0:8080`).

## Database Schema

The service ensures the following schema (executed on startup):

```sql
CREATE TABLE IF NOT EXISTS lots (
    id SERIAL PRIMARY KEY,
    name VARCHAR(255) NOT NULL,
    description TEXT,
    start_price NUMERIC(12, 2) NOT NULL,
    current_price NUMERIC(12, 2),
    owner_id VARCHAR(255),
    created_at TIMESTAMPTZ DEFAULT CURRENT_TIMESTAMP,
    auction_end_date TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_lots_owner_id ON lots(owner_id);
CREATE INDEX IF NOT EXISTS idx_lots_auction_end_date ON lots(auction_end_date);
```

## Docker

Build and run using the provided multi-stage Dockerfile:

```bash
docker build -t auction-service .
docker run --rm -p 8080:8080 \
  -e SUPABASE_HOST=... \
  -e SUPABASE_DB=... \
  -e SUPABASE_USER=... \
  -e SUPABASE_PASSWORD=... \
  -e SUPABASE_PORT=... \
  -e PAYMENT_SERVICE_URL=https://payments.example.com \
  -e SERVICE_REGISTRY_URL=https://registry.example.com/register \
  auction-service
```

## Deploy on Railway

1. Log in and initialize:
   ```bash
   railway login
   railway init
   ```
2. Configure environment variables (`railway variables set ...`) for all Supabase credentials, `PAYMENT_SERVICE_URL`, `SERVICE_REGISTRY_URL`, and optional `SERVER_PORT`.
3. Deploy:
   ```bash
   railway up
   ```

Railway will build the Docker image and run the `auction_service` binary.

## API Endpoints

> All endpoints except `GET /health` require `Authorization: Bearer <token>` header. Tokens are verified by `POST <PAYMENT_SERVICE_URL>/verify` with payload `{"token": "<token>"}`.

| Method | Path | Description |
|--------|------|-------------|
| `GET` | `/health` | Health check (no auth) |
| `GET` | `/lots` | List all lots |
| `GET` | `/lots/{id}` | Retrieve a lot |
| `POST` | `/lots` | Create a lot |
| `POST` | `/lots/{id}/bid` | Place a bid on a lot |

### Sample Requests

```bash
# Health (no token required)
curl http://localhost:8080/health

# List lots
curl -H "Authorization: Bearer $TOKEN" http://localhost:8080/lots

# Create lot
curl -X POST http://localhost:8080/lots \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{
    "name": "Vintage Watch",
    "description": "Swiss made, 1970s",
    "start_price": 150.00,
    "owner_id": "user-123",
    "auction_end_date": "2025-12-31T18:00:00+00"
  }'

# Place bid
curl -X POST http://localhost:8080/lots/1/bid \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{ "amount": 200.00 }'

# Get single lot
curl -H "Authorization: Bearer $TOKEN" http://localhost:8080/lots/1
```

Responses are JSON-encoded and include validation errors where applicable.

