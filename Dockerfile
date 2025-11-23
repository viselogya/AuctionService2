FROM ubuntu:22.04 AS build

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      build-essential \
      cmake \
      libpq-dev \
      libcurl4-openssl-dev \
      git \
      ca-certificates && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY . /app

RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build --target auction_service --config Release

FROM ubuntu:22.04

RUN apt-get update && \
    apt-get install -y --no-install-recommends \
      libpq-dev \
      libcurl4-openssl-dev && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

COPY --from=build /app/build/auction_service /app/auction_service
COPY --from=build /app/README.md /app/README.md

EXPOSE 8080

ENV SERVER_HOST=0.0.0.0
ENV SERVER_PORT=8080

CMD ["./auction_service"]

