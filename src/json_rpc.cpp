
#include "dwn/json_rpc.hpp"
#include "dwn/errors.hpp"
#include "dwn/did.hpp"
#include "dwn/utils.hpp"
#include "dwn/storage.hpp"
#include "dwn/auth.hpp"
#include "dwn/records.hpp"
#include "dwn/config.hpp"
#include <iostream>

namespace dwn {

JsonRpcRequest parse_json_rpc(const std::string& body, size_t maxBytes){
    if(body.size()>maxBytes) throw DwnError(ErrorCode::PayloadTooLarge, 413, "Payload too large");
    nlohmann::json j;
    try { j = nlohmann::json::parse(body); } catch(...){ throw DwnError(ErrorCode::InvalidJsonRpc, 400, "Invalid JSON"); }
    return parse_json_rpc(j);
}

JsonRpcRequest parse_json_rpc(const nlohmann::json& j){
    if(!j.is_object()) throw DwnError(ErrorCode::InvalidJsonRpc, 400, "Invalid Request: not object");
    if(!j.contains("jsonrpc") || j["jsonrpc"]!="2.0") throw DwnError(ErrorCode::InvalidJsonRpc, 400, "Invalid jsonrpc version");
    if(!j.contains("method")) throw DwnError(ErrorCode::InvalidJsonRpc, 400, "Missing method");
    JsonRpcRequest req;
    req.jsonrpc = "2.0";
    req.id = j.value("id", nlohmann::json(nullptr));
    req.method = j["method"].get<std::string>();
    req.params = j.value("params", nlohmann::json::object());
    return req;
}

JsonRpcResponse make_success(const nlohmann::json& id, const nlohmann::json& result){
    JsonRpcResponse r; r.id = id; r.result = result; r.is_error=false; return r;
}
JsonRpcResponse make_error(const nlohmann::json& id, int code, const std::string& msg, const nlohmann::json& data){
    JsonRpcResponse r; r.id=id; r.is_error=true;
    nlohmann::json e; e["code"]=code; e["message"]=msg; if(!data.is_null()) e["data"]=data;
    r.error = e; return r;
}
nlohmann::json response_to_json(const JsonRpcResponse& resp){
    nlohmann::json j; j["jsonrpc"]="2.0"; j["id"]=resp.id;
    if(resp.is_error) j["error"]=resp.error; else j["result"]=resp.result;
    return j;
}

void JsonRpcDispatcher::register_method(const std::string& name, HandlerFn fn){
    handlers_[name]=fn;
}

JsonRpcResponse JsonRpcDispatcher::dispatch(const JsonRpcRequest& req) const {
    auto it = handlers_.find(req.method);
    if(it==handlers_.end()){
        return make_error(req.id, (int)ErrorCode::MethodNotFound, "Method not found: "+req.method);
    }
    try { return it->second(req); }
    catch(const DwnError& e){ return make_error(req.id, (int)e.code, e.what(), e.detail); }
    catch(const std::exception& ex){ return make_error(req.id, (int)ErrorCode::InternalError, ex.what()); }
}

JsonRpcResponse JsonRpcDispatcher::handle_process_message(const JsonRpcRequest& req){
    // Validate params: {target, message, encodedData?}
    if(!req.params.is_object()) throw DwnError(ErrorCode::InvalidParams, 400, "params must be object");
    if(!req.params.contains("target")) throw DwnError(ErrorCode::InvalidParams, 400, "Missing target DID");
    if(!req.params.contains("message")) throw DwnError(ErrorCode::InvalidParams, 400, "Missing message");

    std::string target = req.params["target"].get<std::string>();
    if(!validateDid(target)) throw DwnError(ErrorCode::InvalidParams, 400, "Invalid target DID: "+target);

    auto msgJson = req.params["message"];
    if(!msgJson.is_object()) throw DwnError(ErrorCode::InvalidParams, 400, "message must be object");

    // encodedData is optional base64url
    std::optional<std::vector<uint8_t>> encodedData;
    if(req.params.contains("encodedData") && !req.params["encodedData"].is_null()){
        std::string ed = req.params["encodedData"].get<std::string>();
        if(!utils::is_valid_base64url(ed)) throw DwnError(ErrorCode::InvalidParams, 400, "Invalid encodedData base64url");
        try { encodedData = utils::base64url_decode(ed); }
        catch(...){ throw DwnError(ErrorCode::InvalidParams, 400, "Failed to decode encodedData"); }
        if(config_ && encodedData->size() > config_->max_record_data_bytes){
            throw DwnError(ErrorCode::PayloadTooLarge, 413, "Record data too large");
        }
    }

    Message msg;
    try { msg = message_from_json(msgJson, target, encodedData); }
    catch(const std::exception& e){ throw DwnError(ErrorCode::InvalidParams, 400, std::string("Invalid message: ")+e.what()); }

    if(msg.descriptor.interfaceName.empty() || msg.descriptor.method.empty()){
        throw DwnError(ErrorCode::InvalidParams, 400, "Missing interface/method in descriptor");
    }

    // Only Records interface for MVP
    if(msg.descriptor.interfaceName!="Records"){
        return make_success(req.id, nlohmann::json{{"reply", {{"status", {{"code",400},{"detail","Unsupported interface: "+msg.descriptor.interfaceName}}}}}});
    }

    if(!storage_ || !auth_) throw DwnError(ErrorCode::InternalError, 500, "Server not initialized");

    RecordsHandler handler(*storage_, *auth_);
    RecordsResult res;
    if(msg.descriptor.method=="Write") res = handler.handleWrite(msg);
    else if(msg.descriptor.method=="Read") res = handler.handleRead(msg);
    else if(msg.descriptor.method=="Query") res = handler.handleQuery(msg);
    else if(msg.descriptor.method=="Delete") res = handler.handleDelete(msg);
    else {
        return make_success(req.id, nlohmann::json{{"reply", {{"status", {{"code",400},{"detail","Unsupported method: "+msg.descriptor.method}}}}}});
    }

    nlohmann::json result;
    result["reply"] = {
        {"status", {{"code", res.statusCode},{"detail", res.detail}}}
    };
    if(!res.data.is_null() && !res.data.empty()) result["reply"]["entries"] = res.data; // legacy
    // DWN spec: result.reply may contain record or entries
    if(res.data.contains("record")) result["reply"]["record"] = res.data["record"];
    if(res.data.contains("encodedData")) result["reply"]["encodedData"] = res.data["encodedData"];
    if(res.data.contains("entries")) result["reply"]["entries"] = res.data["entries"];
    if(res.data.contains("count")) result["reply"]["count"] = res.data["count"];
    if(res.data.contains("recordId")) result["reply"]["recordId"] = res.data["recordId"];
    if(res.data.contains("dataCid")) result["reply"]["dataCid"] = res.data["dataCid"];
    // Also put full data for debugging in MVP
    result["reply"]["result"] = res.data;

    return make_success(req.id, result);
}

} // namespace dwn
