
#include "dwn/message.hpp"
#include "dwn/utils.hpp"
#include <stdexcept>

namespace dwn {

nlohmann::json descriptor_to_json(const MessageDescriptor& d){
    nlohmann::json j;
    j["interface"] = d.interfaceName;
    j["method"] = d.method;
    if(d.dataCid) j["dataCid"] = *d.dataCid;
    if(d.dataSize) j["dataSize"] = *d.dataSize;
    j["dateCreated"] = d.dateCreated;
    j["dateModified"] = d.dateModified;
    j["dataFormat"] = d.dataFormat;
    if(d.recordId) j["recordId"] = *d.recordId;
    if(d.schema) j["schema"] = *d.schema;
    if(d.protocol) j["protocol"] = *d.protocol;
    if(d.protocolPath) j["protocolPath"] = *d.protocolPath;
    if(d.recipient) j["recipient"] = *d.recipient;
    if(d.published) j["published"] = *d.published;
    if(!d.tags.empty()) j["tags"] = d.tags;
    return j;
}

MessageDescriptor descriptor_from_json(const nlohmann::json& j){
    MessageDescriptor d;
    d.interfaceName = j.value("interface", j.value("interfaceName",""));
    d.method = j.value("method","");
    if(j.contains("dataCid")) d.dataCid = j["dataCid"].get<std::string>();
    if(j.contains("dataSize")) d.dataSize = j["dataSize"].get<uint64_t>();
    d.dateCreated = j.value("dateCreated", utils::now_iso8601());
    d.dateModified = j.value("dateModified", d.dateCreated);
    d.dataFormat = j.value("dataFormat","application/json");
    if(j.contains("recordId")) d.recordId = j["recordId"].get<std::string>();
    if(j.contains("schema")) d.schema = j["schema"].get<std::string>();
    if(j.contains("protocol")) d.protocol = j["protocol"].get<std::string>();
    if(j.contains("protocolPath")) d.protocolPath = j["protocolPath"].get<std::string>();
    if(j.contains("recipient")) d.recipient = j["recipient"].get<std::string>();
    if(j.contains("published")) d.published = j["published"].get<bool>();
    if(j.contains("tags")) d.tags = j["tags"];
    return d;
}

Authorization auth_from_json(const nlohmann::json& j){
    Authorization a;
    a.payload = j.value("payload","");
    if(j.contains("signatures")){
        for(auto& s: j["signatures"]){
            Signature sig;
            sig.protectedHeader = s.value("protected","");
            sig.signature = s.value("signature","");
            sig.kid = s.value("kid","");
            if(sig.kid.empty() && s.contains("protected")){
                try {
                    auto dec = utils::base64url_decode(s["protected"].get<std::string>());
                    std::string prot(dec.begin(), dec.end());
                    auto pj = nlohmann::json::parse(prot);
                    if(pj.contains("kid")) sig.kid = pj["kid"].get<std::string>();
                } catch(...) {}
            }
            a.signatures.push_back(sig);
        }
    }
    return a;
}

nlohmann::json auth_to_json(const Authorization& a){
    nlohmann::json j;
    j["payload"] = a.payload;
    j["signatures"] = nlohmann::json::array();
    for(auto& s: a.signatures){
        nlohmann::json sj;
        sj["protected"] = s.protectedHeader;
        sj["signature"] = s.signature;
        if(!s.kid.empty()) sj["kid"] = s.kid;
        j["signatures"].push_back(sj);
    }
    return j;
}

Message message_from_json(const nlohmann::json& j, const std::string& targetDid, const std::optional<std::vector<uint8_t>>& encodedData){
    Message m;
    m.targetDid = targetDid;
    m.raw = j;
    if(j.contains("recordId")) m.recordId = j["recordId"].get<std::string>();
    else if(j.contains("descriptor") && j["descriptor"].contains("recordId")) m.recordId = j["descriptor"]["recordId"].get<std::string>();
    else m.recordId = "";

    if(j.contains("descriptor")) m.descriptor = descriptor_from_json(j["descriptor"]);
    else throw std::runtime_error("missing descriptor");

    if(j.contains("authorization")) m.authorization = auth_from_json(j["authorization"]);
    else m.authorization = Authorization{};

    m.encodedData = encodedData;
    return m;
}

} // namespace dwn
