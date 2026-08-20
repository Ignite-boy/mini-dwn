
#include "dwn/did.hpp"
#include "dwn/utils.hpp"
#include <regex>
#include <algorithm>

namespace dwn {

DidParseResult parseDid(const std::string& didStr) {
    static const std::regex didRegex(R"(^did:([a-z0-9]+):([a-zA-Z0-9._:%\-/]+)$)");
    std::smatch m;
    if(!std::regex_match(didStr, m, didRegex)) {
        return {false, {}, "Invalid DID format"};
    }
    std::string method = m[1];
    std::string id = m[2];
    if(method.empty() || id.empty()) return {false, {}, "Empty method or id"};
    Did d; d.method = method; d.id = id; d.original = normalizeDid(didStr);
    return {true, d, ""};
}

bool validateDid(const std::string& didStr) {
    return parseDid(didStr).ok;
}

std::string normalizeDid(const std::string& didStr) {
    auto pos1 = didStr.find(':');
    if(pos1==std::string::npos) return didStr;
    auto pos2 = didStr.find(':', pos1+1);
    if(pos2==std::string::npos) return didStr;
    std::string prefix = didStr.substr(0, pos1);
    std::string method = didStr.substr(pos1+1, pos2-pos1-1);
    std::string rest = didStr.substr(pos2+1);
    std::transform(method.begin(), method.end(), method.begin(), ::tolower);
    return prefix + ":" + method + ":" + rest;
}

std::optional<std::vector<uint8_t>> extract_ed25519_pubkey_from_did_key(const std::string& didKey) {
    if(didKey.rfind("did:key:z",0)!=0) return std::nullopt;
    std::string b58 = didKey.substr(8);
    if(b58.empty() || b58[0]!='z') return std::nullopt;
    std::string b58body = b58.substr(1);
    try {
        auto decoded = utils::base58_decode(b58body);
        if(decoded.size()<2) return std::nullopt;
        if(decoded[0]!=0xED || decoded[1]!=0x01) return std::nullopt;
        if(decoded.size()!=34) return std::nullopt;
        std::vector<uint8_t> pub(decoded.begin()+2, decoded.end());
        return pub;
    } catch(...) { return std::nullopt; }
}

std::string did_from_kid(const std::string& kid) {
    auto hashPos = kid.find('#');
    if(hashPos==std::string::npos) return kid;
    return kid.substr(0, hashPos);
}

} // namespace dwn
