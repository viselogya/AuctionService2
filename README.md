# Auction Service

Сервис аукционов на C++20. Реализует CRUD и аукционные операции со ставками, работает с Supabase/PostgreSQL через `libpq`, регистрирует методы в сервис-реестре и проверяет токены через внешний платежный сервис.

## Структура проекта

```
/include/auction     Публичные заголовки
/src                 Реализация (core, repository, service, api)
main.cpp             Точка входа приложения
Dockerfile           Многоэтапная сборка Docker
CMakeLists.txt       Конфигурация CMake
README.md            Документация
```

## Переменные окружения

| Переменная | Назначение | Обязательно |
|------------|------------|-------------|
| `SUPABASE_HOST` | Хост Supabase/PostgreSQL | Да |
| `SUPABASE_DB` | Имя базы данных | Да |
| `SUPABASE_USER` | Пользователь БД | Да |
| `SUPABASE_PASSWORD` | Пароль БД | Да |
| `SUPABASE_PORT` | Порт БД | Да |
| `SERVICE_REGISTRY_URL` | Базовый URL сервис-реестра (POST `/service`) | Необязательно (`http://localhost:9000`) |
| `PAYMENT_SERVICE_URL` | Базовый URL платежного сервиса (эндпоинты `/bill`, `/pay`, `/token/check`) | Необязательно (`http://localhost:8081`) |
| `SERVICE_NAME` | Имя сервиса при регистрации и проверке токенов | Необязательно (`AuctionService`) |
| `SERVER_HOST` | Хост HTTP-сервера | Необязательно (`0.0.0.0`) |
| `SERVER_PORT` | Порт HTTP-сервера | Необязательно (`8080`) |

### Пример для вашей Supabase БД

Строка подключения:  
`postgresql://postgres.ticisxmomoofsumkolha:SomeHardPassword@aws-1-eu-central-2.pooler.supabase.com:5432/postgres`

Соответственно:

```bash
export SUPABASE_HOST=aws-1-eu-central-2.pooler.supabase.com
export SUPABASE_DB=postgres
export SUPABASE_USER=postgres.ticisxmomoofsumkolha
export SUPABASE_PASSWORD=SomeHardPassword
export SUPABASE_PORT=5432
```

## Требования для сборки

- Компилятор с поддержкой C++20 (GCC 11+, Clang 13+, MSVC 19.3+)
- CMake 3.18+
- Заголовки и библиотеки `libpq`
- Заголовки и библиотеки `libcurl`

### Конфигурация и сборка

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --target auction_service
```

### Запуск локально

```bash
export SUPABASE_HOST=...
export SUPABASE_DB=...
export SUPABASE_USER=...
export SUPABASE_PASSWORD=...
export SUPABASE_PORT=...
# по желанию:
export PAYMENT_SERVICE_URL=https://payments.example.com
export SERVICE_REGISTRY_URL=https://registry.example.com
./build/auction_service
```

По умолчанию сервис слушает `SERVER_HOST:SERVER_PORT` (`0.0.0.0:8080`).

## Схема базы данных

При старте сервис гарантирует наличие таблицы и индексов:

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

Сборка и запуск через готовый Dockerfile:

```bash
docker build -t auction-service .
docker run --rm -p 8080:8080 \
  -e SUPABASE_HOST=... \
  -e SUPABASE_DB=... \
  -e SUPABASE_USER=... \
  -e SUPABASE_PASSWORD=... \
  -e SUPABASE_PORT=... \
  -e PAYMENT_SERVICE_URL=https://payments.example.com \
  -e SERVICE_REGISTRY_URL=https://registry.example.com \
  auction-service
```

## Деплой на Railway

1. Авторизация и инициализация проекта:
   ```bash
   railway login
   railway init
   ```
2. Задайте переменные окружения (`railway variables set ...`) для Supabase, `PAYMENT_SERVICE_URL`, `SERVICE_REGISTRY_URL` и при необходимости `SERVER_PORT`.
3. Деплой:
   ```bash
   railway up
   ```

Railway соберёт Docker-образ и запустит бинарник `auction_service`.

## HTTP API

> Все методы, кроме `GET /health`, требуют заголовок `Authorization: Bearer <token>`. Токен проверяется запросом `POST <PAYMENT_SERVICE_URL>/verify` с телом `{"token": "<token>"}`.

| Метод | Путь | Описание |
|-------|------|----------|
| `GET` | `/health` | Health-check (без авторизации) |
| `GET` | `/lots` | Список всех лотов |
| `GET` | `/lots/{id}` | Получить лот по идентификатору |
| `POST` | `/lots` | Создать лот |
| `PUT` | `/lots/{id}` | Обновить лот |
| `DELETE` | `/lots/{id}` | Удалить лот |
| `POST` | `/lots/{id}/bid` | Сделать ставку на лот |

### Примеры запросов

```bash
# Health (без токена)
curl http://localhost:8080/health

# Список лотов
curl -H "Authorization: Bearer $TOKEN" http://localhost:8080/lots

# Создать лот
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

# Обновить лот
curl -X PUT http://localhost:8080/lots/1 \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{ "name": "Updated name", "owner_id": "user-456" }'

# Сделать ставку
curl -X POST http://localhost:8080/lots/1/bid \
  -H "Authorization: Bearer $TOKEN" \
  -H "Content-Type: application/json" \
  -d '{ "amount": 200.00 }'

# Удалить лот
curl -X DELETE -H "Authorization: Bearer $TOKEN" http://localhost:8080/lots/1
```

Все ответы возвращаются в формате JSON и содержат сообщения об ошибках при неверных данных.

