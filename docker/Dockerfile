FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential cmake pkg-config libboost-all-dev libssl-dev libsodium-dev libpqxx-dev nlohmann-json3-dev libgtest-dev git && rm -rf /var/lib/apt/lists/*
WORKDIR /app
COPY . .
RUN cmake -S . -B build -DBUILD_TESTS=OFF && cmake --build build -j$(nproc)
FROM ubuntu:22.04
RUN apt-get update && apt-get install -y libboost-system1.74.0 libssl3 libsodium23 libpqxx-6.4 libpq5 && rm -rf /var/lib/apt/lists/*
COPY --from=builder /app/build/mini-dwn /usr/local/bin/mini-dwn
COPY --from=builder /app/migrations /migrations
EXPOSE 3000
CMD ["mini-dwn"]
