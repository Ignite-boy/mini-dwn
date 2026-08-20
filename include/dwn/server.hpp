#pragma once
#include "config.hpp"
#include "storage.hpp"
#include "auth.hpp"
#include "json_rpc.hpp"
#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <memory>
#include <thread>
#include <atomic>
#include <unordered_map>

namespace dwn {

class DwnServer {
public:
    DwnServer(const Config& cfg, Storage& storage, AuthVerifier& auth);
    ~DwnServer();

    void run();
    void stop();

    nlohmann::json health_json();
    nlohmann::json info_json();
    nlohmann::json metrics_json();

private:
    Config config_;
    Storage& storage_;
    AuthVerifier& auth_;
    JsonRpcDispatcher dispatcher_;

    boost::asio::io_context ioc_;
    std::unique_ptr<boost::asio::ip::tcp::acceptor> acceptor_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> workers_;

    void do_accept();
    void handle_session(boost::asio::ip::tcp::socket socket);
    void handle_http(boost::beast::http::request<boost::beast::http::string_body>& req,
                     boost::beast::http::response<boost::beast::http::string_body>& res);

    struct RateBucket { int count; std::chrono::steady_clock::time_point window_start; };
    std::unordered_map<std::string, RateBucket> rate_buckets_;
    std::mutex rate_mu_;
    bool check_rate_limit(const std::string& ip);
};

} // namespace dwn
