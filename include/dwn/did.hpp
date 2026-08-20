#pragma once
#include <string>
#include <optional>
#include <regex>
#include <cstdint>

namespace dwn {

struct Did {
    std::string method; // e.g. "key", "example", "web"
    std::string id;     // method-specific-id
    std::string original; // full did string normalized

    std::string to_string() const { return original; }
};

struct DidParseResult {
    bool ok;
    Did did;
    std::string error;
};

// parseDid: supports generic did:<method>:<id> with validation
DidParseResult parseDid(const std::string& didStr);

// validateDid: returns true if syntactically valid per W3C DID Core (simplified)
bool validateDid(const std::string& didStr);

// normalizeDid: lowercases method, keeps id as-is (per spec, method case-insensitive)
std::string normalizeDid(const std::string& didStr);

// Helpers for did:key Ed25519 extraction
// Returns 32-byte pubkey if valid did:key Ed25519, else nullopt
std::optional<std::vector<uint8_t>> extract_ed25519_pubkey_from_did_key(const std::string& didKey);

// Extract DID from kid like "did:key:z6M...#key-1" -> "did:key:z6M..."
std::string did_from_kid(const std::string& kid);

} // namespace dwn
