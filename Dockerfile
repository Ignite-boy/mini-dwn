FROM ubuntu:24.04 AS builder

RUN apt-get update && apt-get install -y \
    build-essential cmake pkg-config git \
    libboost-system-dev libboost-thread-dev \
    libssl-dev libsodium-dev libpq-dev \
    nlohmann-json3-dev \
    && rm -rf /var/lib/apt/lists/*

# Build the exact libpqxx version used by the Mini-DWN source.
RUN git clone --depth 1 --branch 7.10.0 \
    https://github.com/jtv/libpqxx.git /tmp/libpqxx && \
    cmake -S /tmp/libpqxx -B /tmp/libpqxx/build \
      -DBUILD_TEST=OFF \
      -DSKIP_BUILD_TEST=ON \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_SHARED_LIBS=ON \
      -DCMAKE_INSTALL_PREFIX=/opt/pqxx && \
    cmake --build /tmp/libpqxx/build -j"$(nproc)" && \
    cmake --install /tmp/libpqxx/build

WORKDIR /app
COPY . .

RUN rm -rf build && \
    PKG_CONFIG_PATH=/opt/pqxx/lib/pkgconfig \
    CMAKE_PREFIX_PATH=/opt/pqxx \
    cmake -S . -B build -DBUILD_TESTS=OFF && \
    cmake --build build -j"$(nproc)"

FROM ubuntu:24.04

RUN apt-get update && apt-get install -y \
    libboost-system1.83.0 libboost-thread1.83.0 \
    libssl3 libsodium23 libpq5 \
    && rm -rf /var/lib/apt/lists/*

COPY --from=builder /opt/pqxx/lib/libpqxx-7.10.so /usr/local/lib/libpqxx-7.10.so
RUN ln -sf /usr/local/lib/libpqxx-7.10.so /usr/local/lib/libpqxx.so
COPY --from=builder /app/build/mini-dwn /usr/local/bin/mini-dwn
COPY --from=builder /app/migrations /migrations

RUN ldconfig /usr/local/lib

EXPOSE 10000
ENV DWN_HOST=0.0.0.0
ENV DWN_PORT=10000
CMD ["sh", "-c", "DWN_PORT=${PORT:-10000} mini-dwn"]
