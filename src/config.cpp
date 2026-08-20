
#include "dwn/config.hpp"
#include <cstdlib>
#include <stdexcept>

namespace dwn {

Config Config::from_env() {
    Config c;
    auto get = [](const char* k){ const char* v = std::getenv(k); return v ? std::string(v) : std::string(); };
    auto s = get("DWN_HOST"); if(!s.empty()) c.host = s;
    auto p = get("DWN_PORT"); if(!p.empty()) c.port = static_cast<uint16_t>(std::stoi(p));
    auto l = get("DWN_LOG_LEVEL"); if(!l.empty()) c.log_level = l;
    auto pg = get("DATABASE_URL"); if(!pg.empty()) c.postgres_conn_str = pg;
    else {
        auto pg2 = get("POSTGRES_CONN"); if(!pg2.empty()) c.postgres_conn_str = pg2;
    }
    auto tls = get("DWN_TLS_ENABLED"); if(tls=="1" || tls=="true") c.tls_enabled = true;
    auto cert = get("DWN_TLS_CERT"); if(!cert.empty()) c.tls_cert_path = cert;
    auto key = get("DWN_TLS_KEY"); if(!key.empty()) c.tls_key_path = key;
    auto maxReq = get("DWN_MAX_REQUEST_BYTES"); if(!maxReq.empty()) c.max_request_bytes = std::stoul(maxReq);
    auto maxRec = get("DWN_MAX_RECORD_BYTES"); if(!maxRec.empty()) c.max_record_data_bytes = std::stoul(maxRec);
    auto rl = get("DWN_RATE_LIMIT"); if(!rl.empty()) c.rate_limit_per_min = std::stoi(rl);
    auto name = get("DWN_SERVER_NAME"); if(!name.empty()) c.server_name = name;
    return c;
}

void Config::validate() const {
    if(port==0) throw std::runtime_error("invalid port");
    if(postgres_conn_str.empty()) throw std::runtime_error("postgres conn string empty");
    if(tls_enabled && (tls_cert_path.empty() || tls_key_path.empty()))
        throw std::runtime_error("TLS enabled but cert/key missing");
    if(max_request_bytes < 1024) throw std::runtime_error("max_request_bytes too small");
}

} // namespace dwn
