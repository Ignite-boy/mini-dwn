#pragma once
#include <string>
#include <cstdint>
#include <optional>

namespace dwn {

struct Config {
    std::string host = "0.0.0.0";
    uint16_t port = 3000;
    std::string log_level = "info";

    std::string postgres_conn_str = "postgresql://dwn:dwn_password@localhost:5432/mini_dwn";

    // TLS
    bool tls_enabled = false;
    std::string tls_cert_path;
    std::string tls_key_path;

    // Limits
    size_t max_request_bytes = 4 * 1024 * 1024; // 4 MB
    size_t max_record_data_bytes = 2 * 1024 * 1024; // 2 MB
    int rate_limit_per_min = 120;

    // Server identity
    std::string server_name = "milan-mini-dwn";
    std::string version = "0.1.0";

    static Config from_env();
    void validate() const;
};

} // namespace dwn
