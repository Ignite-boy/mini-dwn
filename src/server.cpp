
#include "dwn/server.hpp"
#include "dwn/errors.hpp"
#include "dwn/utils.hpp"
#include "dwn/postgres_storage.hpp"
#include <boost/beast/core.hpp>
#include <boost/beast/http.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/strand.hpp>
#include <iostream>
#include <chrono>

namespace dwn {
namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
using tcp = net::ip::tcp;

DwnServer::DwnServer(const Config& cfg, Storage& storage, AuthVerifier& auth)
    : config_(cfg), storage_(storage), auth_(auth), ioc_(1) {
    dispatcher_.set_storage(&storage_);
    dispatcher_.set_auth(&auth_);
    dispatcher_.set_config(&config_);
    dispatcher_.register_method("dwn.processMessage",
        [this](const JsonRpcRequest& req){ return dispatcher_.handle_process_message(req); });
}

DwnServer::~DwnServer(){ stop(); }

nlohmann::json DwnServer::health_json(){
    bool db = storage_.health_check();
    return {{"ok", db}, {"service","mini-dwn"}, {"storage","postgresql"}, {"database", db}};
}
nlohmann::json DwnServer::info_json(){
    return {
        {"server", config_.server_name},
        {"version", config_.version},
        {"protocol","DWN"},
        {"websocketSupport", true},
        {"storage","postgresql"}
    };
}
nlohmann::json DwnServer::metrics_json(){
    return {{"uptime","unknown"},{"rateLimitPerMin", config_.rate_limit_per_min},{"maxRequestBytes", config_.max_request_bytes}};
}

void DwnServer::handle_database_snapshot(
    const http::request<http::string_body>& req,
    http::response<http::string_body>& res,
    const std::string& name){

    auto* pg = dynamic_cast<PostgresStorage*>(&storage_);
    if(!pg){
        res.result(http::status::not_implemented);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"ok":false,"error":"Postgres snapshot backend unavailable"})";
        res.prepare_payload();
        return;
    }

    try {
        if(req.method() == http::verb::put){
            auto body = nlohmann::json::parse(req.body());

            nlohmann::json data =
                body.contains("data") ? body["data"] : body;

            std::string pushedAt =
                body.value("pushedAt", std::string());

            if(!pg->put_database_snapshot(name, data, pushedAt)){
                res.result(http::status::internal_server_error);
                res.set(http::field::content_type, "application/json");
                res.body() = R"({"ok":false,"error":"snapshot write failed"})";
                res.prepare_payload();
                return;
            }

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{
                {"ok", true},
                {"stored", true},
                {"name", name},
                {"pushedAt", pushedAt.empty() ? nlohmann::json(nullptr) : nlohmann::json(pushedAt)}
            }.dump();
            res.prepare_payload();
            return;
        }

        if(req.method() == http::verb::get){
            std::string pushedAt;
            auto data = pg->get_database_snapshot(name, &pushedAt);

            if(!data){
                res.result(http::status::not_found);
                res.set(http::field::content_type, "application/json");
                res.body() = nlohmann::json{
                    {"ok", false},
                    {"error", "snapshot not found"},
                    {"name", name}
                }.dump();
                res.prepare_payload();
                return;
            }

            res.result(http::status::ok);
            res.set(http::field::content_type, "application/json");
            res.body() = nlohmann::json{
                {"ok", true},
                {"name", name},
                {"data", *data},
                {"pushedAt", pushedAt}
            }.dump();
            res.prepare_payload();
            return;
        }

        res.result(http::status::method_not_allowed);
        res.set(http::field::content_type, "application/json");
        res.body() = R"({"ok":false,"error":"method not allowed"})";
        res.prepare_payload();

    } catch(const std::exception& e){
        res.result(http::status::bad_request);
        res.set(http::field::content_type, "application/json");
        res.body() = nlohmann::json{
            {"ok", false},
            {"error", std::string("snapshot request failed: ") + e.what()}
        }.dump();
        res.prepare_payload();
    }
}

bool DwnServer::check_rate_limit(const std::string& ip){
    std::lock_guard<std::mutex> lock(rate_mu_);
    auto now = std::chrono::steady_clock::now();
    auto& b = rate_buckets_[ip];
    if(std::chrono::duration_cast<std::chrono::minutes>(now - b.window_start).count() >=1){
        b.window_start = now; b.count=0;
    }
    b.count++;
    return b.count <= config_.rate_limit_per_min;
}

void DwnServer::handle_http(http::request<http::string_body>& req, http::response<http::string_body>& res){
    res.version(req.version());
    res.keep_alive(false);
    std::string target(req.target());

    // Security: request size already limited by beast body limit
    if(target=="/health"){
        res.result(http::status::ok);
        res.set(http::field::content_type,"application/json");
        res.body() = health_json().dump();
        res.prepare_payload(); return;
    }
    if(target=="/info"){
        res.result(http::status::ok);
        res.set(http::field::content_type,"application/json");
        res.body() = info_json().dump();
        res.prepare_payload(); return;
    }
    if(target=="/metrics"){
        res.result(http::status::ok);
        res.set(http::field::content_type,"application/json");
        res.body() = metrics_json().dump();
        res.prepare_payload(); return;
    }
    if(target=="/version"){
        res.result(http::status::ok);
        res.set(http::field::content_type,"application/json");
        res.body() = nlohmann::json{{"version", config_.version}}.dump();
        res.prepare_payload(); return;
    }
    // Milan REST user provisioning compatibility routes.
    const std::vector<std::string> provisionPrefixes = {
        "/api/dwn/users/provision",
        "/api/cloud-dwn/users/provision",
        "/api/dwn/user/provision"
    };

    for(const auto& route : provisionPrefixes){
        if(target == route && req.method() == http::verb::post){
            try {
                auto body = nlohmann::json::parse(req.body());
                const std::string did = body.value("did", "");

                if(did.empty()){
                    res.result(http::status::bad_request);
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"ok":false,"error":"did required"})";
                    res.prepare_payload();
                    return;
                }

                if(!storage_.ensure_tenant(did)){
                    res.result(http::status::internal_server_error);
                    res.set(http::field::content_type, "application/json");
                    res.body() = R"({"ok":false,"error":"tenant provisioning failed"})";
                    res.prepare_payload();
                    return;
                }

                res.result(http::status::ok);
                res.set(http::field::content_type, "application/json");
                res.body() = nlohmann::json{
                    {"ok", true},
                    {"provisioned", true},
                    {"spaceId", body.value("spaceId", "")},
                    {"ownerDid", did},
                    {"did", did},
                    {"userId", body.value("userId", "")},
                    {"email", body.value("email", "")},
                    {"realDwnProtocol", true}
                }.dump();
                res.prepare_payload();
                return;
            } catch(const std::exception& e){
                res.result(http::status::bad_request);
                res.set(http::field::content_type, "application/json");
                res.body() = nlohmann::json{
                    {"ok", false},
                    {"error", std::string("provision failed: ") + e.what()}
                }.dump();
                res.prepare_payload();
                return;
            }
        }
    }

    // Milan REST database snapshot compatibility routes.
    // These map to the same PostgreSQL-backed snapshot store.
    const std::vector<std::string> snapshotPrefixes = {
        "/api/dwn/database/",
        "/api/cloud-dwn/database/",
        "/api/cloud-dwn/db/",
        "/api/dwn/database/write/",
        "/api/dwn/database/read/",
        "/api/dwn/snapshot/",
        "/api/cloud-dwn/node/database/",
        "/api/dwn/db/",
        "/api/dwn/databases/",
        "/api/cloud-dwn/snapshot/",
        "/api/cloud-dwn/snapshots/",
        "/api/dwn/database/snapshot/",
        "/api/cloud-dwn/database/write/",
        "/api/cloud-dwn/database/read/"
    };

    for(const auto& prefix : snapshotPrefixes){
        if(target.rfind(prefix, 0) == 0 && target.size() > prefix.size()){
            const std::string name = target.substr(prefix.size());
            handle_database_snapshot(req, res, name);
            return;
        }
    }

    if(target=="/json-rpc" && req.method()==http::verb::post){
        const std::string authz = std::string(req[http::field::authorization]);
        if(authz != "Bearer milan-v49-embedded-production-dwn-key"){
            res.result(http::status::unauthorized);
            res.set(http::field::content_type,"application/json");
            res.body() = R"({"error":"Unauthorized"})";
            res.prepare_payload();
            return;
        }
        // Rate limit check
        std::string ip = "unknown"; // would extract from socket in handle_session
        // Process JSON-RPC
        try {
            auto jsonReq = parse_json_rpc(req.body(), config_.max_request_bytes);
            auto jsonResp = dispatcher_.dispatch(jsonReq);
            res.result(http::status::ok);
            res.set(http::field::content_type,"application/json");
            res.body() = response_to_json(jsonResp).dump();
        } catch(const DwnError& e){
            res.result(http::status::ok);
            res.set(http::field::content_type,"application/json");
            auto errResp = make_error(nullptr, (int)e.code, e.what(), e.detail);
            res.body() = response_to_json(errResp).dump();
        } catch(const std::exception& ex){
            res.result(http::status::bad_request);
            res.set(http::field::content_type,"application/json");
            res.body() = nlohmann::json{{"error", ex.what()}}.dump();
        }
        res.prepare_payload(); return;
    }
    // 404
    res.result(http::status::not_found);
    res.set(http::field::content_type,"application/json");
    res.body() = nlohmann::json{{"error","Not found"},{"path", target}}.dump();
    res.prepare_payload();
}

void DwnServer::handle_session(tcp::socket socket){
    try {
        beast::flat_buffer buffer;
        // Check if websocket upgrade
        http::request<http::string_body> req;
        http::read(socket, buffer, req);

        std::string ip = socket.remote_endpoint().address().to_string();
        if(!check_rate_limit(ip)){
            http::response<http::string_body> res{http::status::too_many_requests, req.version()};
            res.set(http::field::content_type,"application/json");
            res.body() = R"({"error":"Too many requests"})";
            res.prepare_payload();
            http::write(socket, res);
            return;
        }

        if(websocket::is_upgrade(req)){
            websocket::stream<tcp::socket> ws{std::move(socket)};
            ws.accept(req);
            for(;;){
                beast::flat_buffer wsBuf;
                ws.read(wsBuf);
                std::string msg = beast::buffers_to_string(wsBuf.data());
                try {
                    auto jsonReq = parse_json_rpc(msg, config_.max_request_bytes);
                    auto jsonResp = dispatcher_.dispatch(jsonReq);
                    std::string out = response_to_json(jsonResp).dump();
                    ws.text(true);
                    ws.write(net::buffer(out));
                } catch(...){
                    // send error
                    nlohmann::json err = {{"jsonrpc","2.0"},{"id",nullptr},{"error",{{"code",-32600},{"message","Invalid Request"}}}};
                    ws.write(net::buffer(err.dump()));
                }
            }
        } else {
            http::response<http::string_body> res;
            handle_http(req, res);
            http::write(socket, res);
        }
    } catch(const std::exception& e){
        // session error, log
        std::cerr << "session error: " << e.what() << "\n";
    }
}

void DwnServer::do_accept(){
    acceptor_->async_accept([this](beast::error_code ec, tcp::socket socket){
        if(!ec){
            std::thread([this, s=std::move(socket)]() mutable { handle_session(std::move(s)); }).detach();
        }
        if(running_) do_accept();
    });
}

void DwnServer::run(){
    running_=true;
    auto addr = net::ip::make_address(config_.host);
    tcp::endpoint ep{addr, config_.port};
    acceptor_ = std::make_unique<tcp::acceptor>(ioc_, ep);
    std::cout << "Mini-DWN listening on " << config_.host << ":" << config_.port << "\n";
    do_accept();
    ioc_.run();
}

void DwnServer::stop(){
    running_=false;
    try { ioc_.stop(); if(acceptor_ && acceptor_->is_open()) acceptor_->close(); } catch(...) {}
}

} // namespace dwn
