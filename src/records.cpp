
#include "dwn/records.hpp"
#include "dwn/utils.hpp"
#include "dwn/errors.hpp"
#include <random>

namespace dwn {

RecordsHandler::RecordsHandler(Storage& storage, AuthVerifier& auth)
    : storage_(storage), auth_(auth) {}

static std::string gen_record_id(){
    std::random_device rd; std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0,15);
    std::uniform_int_distribution<> dis2(8,11);
    std::string uuid;
    for(int i=0;i<32;i++){
        if(i==8||i==12||i==16||i==20) uuid+='-';
        int v = (i==12)?4: (i==16)?dis2(gen): dis(gen);
        uuid += "0123456789abcdef"[v];
    }
    return uuid;
}

RecordsResult RecordsHandler::handleWrite(const Message& msg){
    // Auth
    auto authRes = auth_.verifyAuthorization(msg, msg.targetDid);
    if(!authRes.authorized){
        return {401, "Unauthorized: "+authRes.reason, {}};
    }
    if(!auth_.authorizeWrite(msg, authRes.signerDid, msg.targetDid)){
        return {403, "Forbidden: write denied", {}};
    }

    if(!msg.encodedData || msg.encodedData->empty()){
        return {400, "Missing encodedData for RecordsWrite", {}};
    }

    // Verify dataCid if provided
    if(msg.descriptor.dataCid){
        if(!utils::verify_data_cid(*msg.encodedData, *msg.descriptor.dataCid)){
            return {400, "dataCid mismatch", {}};
        }
    }

    std::string recordId = msg.recordId.empty() ? (msg.descriptor.recordId ? *msg.descriptor.recordId : gen_record_id()) : msg.recordId;
    // Build record
    Record rec;
    rec.recordId = recordId;
    rec.targetDid = msg.targetDid;
    rec.ownerDid = authRes.signerDid;
    rec.dataFormat = msg.descriptor.dataFormat;
    rec.schema = msg.descriptor.schema.value_or("");
    rec.protocol = msg.descriptor.protocol.value_or("");
    rec.protocolPath = msg.descriptor.protocolPath.value_or("");
    rec.recipient = msg.descriptor.recipient.value_or("");
    rec.published = msg.descriptor.published.value_or(false);
    rec.dateCreated = msg.descriptor.dateCreated.empty()? utils::now_iso8601(): msg.descriptor.dateCreated;
    rec.dateModified = utils::now_iso8601();
    rec.deleted = false;
    rec.metadata = descriptor_to_json(msg.descriptor);
    rec.data = *msg.encodedData;
    rec.dataSize = rec.data.size();
    rec.dataCid = utils::compute_data_cid(rec.data);

    // Ensure tenant exists
    storage_.ensure_tenant(msg.targetDid);

    bool ok = storage_.write_record(rec);
    if(!ok) return {500, "Storage error", {}};

    storage_.append_event(msg.targetDid, recordId, "RecordsWrite", {{"owner", rec.ownerDid}});

    nlohmann::json data;
    data["recordId"] = recordId;
    data["dataCid"] = rec.dataCid;
    data["dataSize"] = rec.dataSize;
    return {202, "Accepted", data};
}

RecordsResult RecordsHandler::handleRead(const Message& msg){
    auto authRes = auth_.verifyAuthorization(msg, msg.targetDid);
    if(!authRes.authorized) return {401, "Unauthorized: "+authRes.reason, {}};

    std::string recordId = msg.recordId;
    if(recordId.empty() && msg.descriptor.recordId) recordId = *msg.descriptor.recordId;
    if(recordId.empty()) return {400, "Missing recordId for RecordsRead", {}};

    auto recOpt = storage_.read_record(msg.targetDid, recordId);
    if(!recOpt) return {404, "Record not found", {}};
    auto& rec = *recOpt;

    if(!auth_.authorizeRead(msg, authRes.signerDid, rec)){
        return {403, "Forbidden: read denied", {}};
    }

    nlohmann::json data;
    data["record"] = {
        {"recordId", rec.recordId},
        {"descriptor", descriptor_to_json(msg.descriptor)},
        {"dataFormat", rec.dataFormat},
        {"dataCid", rec.dataCid},
        {"dataSize", rec.dataSize},
        {"owner", rec.ownerDid},
        {"published", rec.published}
    };
    // For JSON dataFormat, try to embed as string if possible
    // Return encodedData as base64url for transport (per DWN spec)
    data["encodedData"] = utils::base64url_encode(rec.data);

    return {200, "OK", data};
}

RecordsResult RecordsHandler::handleQuery(const Message& msg){
    auto authRes = auth_.verifyAuthorization(msg, msg.targetDid);
    if(!authRes.authorized) return {401, "Unauthorized: "+authRes.reason, {}};
    if(!auth_.authorizeQuery(msg, authRes.signerDid, msg.targetDid)) return {403, "Forbidden: query denied", {}};

    // Build filter from descriptor
    RecordsFilter filter;
    if(msg.descriptor.schema) filter.schema = msg.descriptor.schema;
    if(!msg.descriptor.dataFormat.empty()) filter.dataFormat = msg.descriptor.dataFormat;
    if(msg.descriptor.protocol) filter.protocol = msg.descriptor.protocol;
    if(msg.descriptor.protocolPath) filter.protocolPath = msg.descriptor.protocolPath;
    if(msg.descriptor.recipient) filter.recipient = msg.descriptor.recipient;
    filter.published = msg.descriptor.published;
    filter.tags = msg.descriptor.tags;
    // Extract author from raw filter if present
    if(msg.raw.contains("filter")){
        auto f = msg.raw["filter"];
        if(f.contains("schema")) filter.schema = f["schema"].get<std::string>();
        if(f.contains("dataFormat")) filter.dataFormat = f["dataFormat"].get<std::string>();
        if(f.contains("protocol")) filter.protocol = f["protocol"].get<std::string>();
        if(f.contains("protocolPath")) filter.protocolPath = f["protocolPath"].get<std::string>();
        if(f.contains("recipient")) filter.recipient = f["recipient"].get<std::string>();
        if(f.contains("published")) filter.published = f["published"].get<bool>();
        if(f.contains("tags")) filter.tags = f["tags"];
        if(f.contains("limit")) filter.limit = f["limit"].get<int>();
        if(f.contains("offset")) filter.offset = f["offset"].get<int>();
    }

    auto results = storage_.query_records(msg.targetDid, filter);

    nlohmann::json entries = nlohmann::json::array();
    for(auto& r: results){
        entries.push_back({
            {"recordId", r.recordId},
            {"descriptor", {
                {"dataFormat", r.dataFormat},
                {"dataCid", r.dataCid},
                {"dataSize", r.dataSize},
                {"dateCreated", r.dateCreated},
                {"schema", r.schema},
                {"protocol", r.protocol}
            }},
            {"owner", r.ownerDid}
        });
    }

    nlohmann::json data;
    data["entries"] = entries;
    data["count"] = entries.size();
    return {200, "OK", data};
}

RecordsResult RecordsHandler::handleDelete(const Message& msg){
    auto authRes = auth_.verifyAuthorization(msg, msg.targetDid);
    if(!authRes.authorized) return {401, "Unauthorized: "+authRes.reason, {}};

    std::string recordId = msg.recordId;
    if(recordId.empty() && msg.descriptor.recordId) recordId = *msg.descriptor.recordId;
    if(recordId.empty()) return {400, "Missing recordId for RecordsDelete", {}};

    auto recOpt = storage_.read_record(msg.targetDid, recordId);
    if(!recOpt) return {404, "Record not found", {}};
    if(!auth_.authorizeDelete(msg, authRes.signerDid, *recOpt)) return {403, "Forbidden: delete denied", {}};

    bool ok = storage_.delete_record(msg.targetDid, recordId, true);
    if(!ok) return {500, "Storage delete failed", {}};

    storage_.append_event(msg.targetDid, recordId, "RecordsDelete", {});

    return {202, "Accepted", {{"recordId", recordId}}};
}

} // namespace dwn
