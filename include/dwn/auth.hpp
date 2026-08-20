#pragma once
#include <string>
#include <optional>
#include "message.hpp"
#include "storage.hpp"

namespace dwn {

// Authorization result
struct AuthResult {
    bool authorized;
    std::string signerDid;
    std::string reason; // if not authorized
};

// Abstracts cryptographic verification
class AuthVerifier {
public:
    AuthVerifier() = default;

    // Verify JWS signatures in Authorization, returns signer DID
    // For MVP we require at least one valid Ed25519 signature where kid's DID matches payload's author
    AuthResult verifyAuthorization(const Message& msg, const std::string& targetDid);

    // Verify a single detached signature over payload
    bool verifySignature(const std::string& payloadB64Url, const Signature& sig, const std::vector<uint8_t>& pubkey);

    // Owner-only policy (MVP)
    bool authorizeRead(const Message& msg, const std::string& signerDid, const Record& record);
    bool authorizeWrite(const Message& msg, const std::string& signerDid, const std::string& targetDid);
    bool authorizeDelete(const Message& msg, const std::string& signerDid, const Record& record);
    bool authorizeQuery(const Message& msg, const std::string& signerDid, const std::string& targetDid);

private:
    std::optional<std::vector<uint8_t>> resolve_pubkey(const std::string& kidOrDid);
};

} // namespace dwn
