#pragma once
#include <stdexcept>
#include <string>
#include <string_view>

namespace dwn {

enum class ErrorCode {
    Ok = 0,
    InvalidJsonRpc = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    // DWN-specific (use JSON-RPC -32000 range + DWN status codes)
    BadRequest = 400,
    Unauthorized = 401,
    Forbidden = 403,
    NotFound = 404,
    Conflict = 409,
    PayloadTooLarge = 413,
    UnsupportedMediaType = 415,
    Unprocessable = 422,
    TooManyRequests = 429,
};

struct DwnError : public std::runtime_error {
    ErrorCode code;
    int dwnStatus; // maps to reply.status.code (e.g. 401, 404)
    std::string detail;

    DwnError(ErrorCode c, int status, std::string_view msg, std::string_view det = "")
        : std::runtime_error(std::string(msg)), code(c), dwnStatus(status), detail(det) {}
};

inline const char* to_string(ErrorCode c) {
    switch(c){
        case ErrorCode::InvalidJsonRpc: return "Invalid Request";
        case ErrorCode::MethodNotFound: return "Method not found";
        case ErrorCode::InvalidParams: return "Invalid params";
        case ErrorCode::InternalError: return "Internal error";
        default: return "Error";
    }
}

// JSON-RPC standard error mapping
struct JsonRpcErrorObj {
    int code;
    std::string message;
    std::string data;
};

} // namespace dwn
