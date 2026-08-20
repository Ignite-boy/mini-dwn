#pragma once
#include <string>
#include <functional>
#include <nlohmann/json.hpp>
#include "message.hpp"

namespace dwn {

struct JsonRpcRequest {
    std::string jsonrpc = "2.0";
    nlohmann::json id; // string or number or null
    std::string method;
    nlohmann::json params;
};

struct JsonRpcResponse {
    std::string jsonrpc = "2.0";
    nlohmann::json id;
    nlohmann::json result; // on success
    nlohmann::json error;  // on error, contains code/message
    bool is_error = false;
};

struct JsonRpcError {
    int code;
    std::string message;
    nlohmann::json data;
};

// Validates raw JSON string -> JsonRpcRequest, throws DwnError on invalid
JsonRpcRequest parse_json_rpc(const std::string& body, size_t maxBytes);
JsonRpcRequest parse_json_rpc(const nlohmann::json& j);

JsonRpcResponse make_success(const nlohmann::json& id, const nlohmann::json& result);
JsonRpcResponse make_error(const nlohmann::json& id, int code, const std::string& msg, const nlohmann::json& data = nullptr);

nlohmann::json response_to_json(const JsonRpcResponse& resp);

// Dispatcher for method routing
class JsonRpcDispatcher {
public:
    using HandlerFn = std::function<JsonRpcResponse(const JsonRpcRequest&)>;

    void register_method(const std::string& name, HandlerFn fn);
    JsonRpcResponse dispatch(const JsonRpcRequest& req) const;

    // Built-in method: dwn.processMessage
    JsonRpcResponse handle_process_message(const JsonRpcRequest& req);

    // Dependencies injected after construction
    void set_storage(class Storage* s) { storage_ = s; }
    void set_auth(class AuthVerifier* a) { auth_ = a; }
    void set_config(const class Config* c) { config_ = c; }

private:
    std::unordered_map<std::string, HandlerFn> handlers_;
    Storage* storage_ = nullptr;
    AuthVerifier* auth_ = nullptr;
    const Config* config_ = nullptr;
};

} // namespace dwn
