#pragma once
#include <string>
#include <vector>
#include <optional>
#include <nlohmann/json.hpp>
#include "did.hpp"

namespace dwn {

struct Signature {
    std::string protectedHeader; // base64url JWS protected header
    std::string signature;       // base64url signature
    std::string kid;             // key id, e.g. did:key:z...#key-1
};

struct Authorization {
    std::string payload; // base64url-encoded descriptor or JWS payload
    std::vector<Signature> signatures;
};

struct MessageDescriptor {
    std::string interfaceName; // "Records"
    std::string method;        // "Write", "Read", "Query", "Delete"
    std::optional<std::string> dataCid;
    std::optional<uint64_t> dataSize;
    std::string dateCreated;
    std::string dateModified;
    std::string dataFormat; // e.g. "application/json"
    // optional filters / metadata
    std::optional<std::string> recordId;
    std::optional<std::string> schema;
    std::optional<std::string> protocol;
    std::optional<std::string> protocolPath;
    std::optional<std::string> recipient;
    std::optional<bool> published;
    nlohmann::json tags = nlohmann::json::object();
};

struct Message {
    std::string recordId;
    std::string targetDid; // tenant
    MessageDescriptor descriptor;
    Authorization authorization;
    // For Write: decoded payload (optional)
    std::optional<std::vector<uint8_t>> encodedData;
    // Additional context
    nlohmann::json raw;
};

struct Record {
    std::string recordId;
    std::string targetDid;
    std::string ownerDid;
    std::string schema;
    std::string dataFormat;
    std::string protocol;
    std::string protocolPath;
    std::string recipient;
    bool published = false;
    std::string dateCreated;
    std::string dateModified;
    bool deleted = false;
    nlohmann::json metadata = nlohmann::json::object();
    std::vector<uint8_t> data;
    std::string dataCid;
    uint64_t dataSize = 0;
    // Event id for stream
    std::string lastEventId;
};

struct RecordsFilter {
    std::optional<std::string> schema;
    std::optional<std::string> dataFormat;
    std::optional<std::string> protocol;
    std::optional<std::string> protocolPath;
    std::optional<std::string> recipient;
    std::optional<std::string> author; // owner_did
    std::optional<bool> published;
    nlohmann::json tags = nlohmann::json::object();
    std::optional<int> limit = 25;
    std::optional<int> offset = 0;
    // date range
    std::optional<std::string> dateCreatedFrom;
    std::optional<std::string> dateCreatedTo;
};

nlohmann::json descriptor_to_json(const MessageDescriptor& d);
MessageDescriptor descriptor_from_json(const nlohmann::json& j);
Authorization auth_from_json(const nlohmann::json& j);
nlohmann::json auth_to_json(const Authorization& a);

Message message_from_json(const nlohmann::json& j, const std::string& targetDid, const std::optional<std::vector<uint8_t>>& encodedData);

} // namespace dwn
