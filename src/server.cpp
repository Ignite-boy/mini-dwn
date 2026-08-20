
#include "dwn/server.hpp"
#include "dwn/errors.hpp"
#include "dwn/utils.hpp"
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
    if(target=="/json-rpc" && req.method()==http::verb::post){
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
