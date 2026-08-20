
#include "dwn/config.hpp"
#include "dwn/postgres_storage.hpp"
#include "dwn/auth.hpp"
#include "dwn/server.hpp"
#include <iostream>
#include <csignal>

static dwn::DwnServer* g_server = nullptr;
void sig_handler(int){ if(g_server) g_server->stop(); }

int main(int argc, char** argv){
    try {
        auto cfg = dwn::Config::from_env();
        cfg.validate();
        std::cout << "Starting " << cfg.server_name << " v" << cfg.version << "\n";
        std::cout << "Postgres: " << cfg.postgres_conn_str.substr(0, cfg.postgres_conn_str.find('@')!=std::string::npos ? cfg.postgres_conn_str.find('@') : 20) << "...\n";

        dwn::PostgresStorage storage(cfg.postgres_conn_str);
        if(!storage.health_check()){
            std::cerr << "Database health check failed\n";
            return 1;
        }
        std::cout << "Database OK\n";

        dwn::AuthVerifier auth;

        dwn::DwnServer server(cfg, storage, auth);
        g_server = &server;
        std::signal(SIGINT, sig_handler);
        std::signal(SIGTERM, sig_handler);

        server.run();
    } catch(const std::exception& e){
        std::cerr << "Fatal: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
