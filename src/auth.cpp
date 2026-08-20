
#include "dwn/auth.hpp"
#include "dwn/did.hpp"
#include "dwn/utils.hpp"
#include <sodium.h>
#include <nlohmann/json.hpp>

namespace dwn {

AuthResult AuthVerifier::verifyAuthorization(const Message& msg, const std::string& targetDid) {
    if(msg.authorization.signatures.empty()){
        // For MVP testing, allow unsigned if targetDid starts with did:example (dev mode)
        // SECURITY NOTE: In production, this must be denied.
        // We check if target is did:example - then treat signer as target itself for owner-only dev
        if(targetDid.rfind("did:example:",0)==0){
            return {true, targetDid, ""};
        }
        return {false, "", "Missing signatures"};
    }

    // For each signature, attempt to verify
    for(const auto& sig: msg.authorization.signatures){
        auto pubOpt = resolve_pubkey(sig.kid);
        if(!pubOpt) continue;
        try {
            if(verifySignature(msg.authorization.payload, sig, *pubOpt)){
                std::string signerDid = did_from_kid(sig.kid);
                // Validate signer DID format
                if(!validateDid(signerDid)) continue;
                return {true, signerDid, ""};
            }
        } catch(...) { continue; }
    }
    return {false, "", "Signature verification failed"};
}

bool AuthVerifier::verifySignature(const std::string& payloadB64Url, const Signature& sig, const std::vector<uint8_t>& pubkey){
    // payloadB64Url is JWS payload (base64url)
    // protectedHeader is base64url
    // signature is base64url of detached signature over ASCII: protected.payload
    if(pubkey.size()!=32) return false;
    std::string signingInput = sig.protectedHeader + "." + payloadB64Url;
    auto sigBytes = utils::base64url_decode(sig.signature);
    if(sigBytes.size()!=64) return false;

    // libsodium expects message and signature
    // crypto_sign_verify_detached(sig, msg, msglen, pubkey)
    if(sodium_init()<0) return false;
    int res = crypto_sign_verify_detached(sigBytes.data(),
                reinterpret_cast<const unsigned char*>(signingInput.data()),
                signingInput.size(),
                pubkey.data());
    return res==0;
}

bool AuthVerifier::authorizeRead(const Message& msg, const std::string& signerDid, const Record& record){
    // Owner-only MVP: signer must equal targetDid and ownerDid
    if(signerDid!=msg.targetDid) return false;
    if(record.ownerDid!=signerDid) return false;
    if(record.deleted) return false;
    return true;
}
bool AuthVerifier::authorizeWrite(const Message& msg, const std::string& signerDid, const std::string& targetDid){
    // Owner-only: signer == target
    return signerDid==targetDid;
}
bool AuthVerifier::authorizeDelete(const Message& msg, const std::string& signerDid, const Record& record){
    return signerDid==msg.targetDid && record.ownerDid==signerDid;
}
bool AuthVerifier::authorizeQuery(const Message& msg, const std::string& signerDid, const std::string& targetDid){
    return signerDid==targetDid;
}

std::optional<std::vector<uint8_t>> AuthVerifier::resolve_pubkey(const std::string& kidOrDid){
    std::string did = did_from_kid(kidOrDid);
    // Only support did:key for MVP
    if(did.rfind("did:key:",0)==0){
        return extract_ed25519_pubkey_from_did_key(did);
    }
    // For did:example etc, we cannot resolve key - in dev mode return nullopt
    return std::nullopt;
}

} // namespace dwn
