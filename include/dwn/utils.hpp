#pragma once
#include <string>
#include <vector>
#include <optional>
#include <array>
#include <cstdint>

namespace dwn {

namespace utils {

// Base64Url
std::string base64url_encode(const std::vector<uint8_t>& data);
std::string base64url_encode(const std::string& s);
std::vector<uint8_t> base64url_decode(const std::string& s);
bool is_valid_base64url(const std::string& s);

// Base58 (Bitcoin alphabet) for did:key decoding
std::vector<uint8_t> base58_decode(const std::string& b58);
std::string base58_encode(const std::vector<uint8_t>& data);

// SHA-256
std::array<uint8_t,32> sha256(const std::vector<uint8_t>& data);
std::array<uint8_t,32> sha256(const std::string& s);
std::string sha256_hex(const std::string& s);
std::string sha256_hex(const std::vector<uint8_t>& data);

// CID abstraction (MVP): dataCid = sha256_hex(data) prefixed with multihash marker
// We do NOT claim full CIDv1 compliance until multicodec is implemented.
std::string compute_data_cid(const std::vector<uint8_t>& data);
bool verify_data_cid(const std::vector<uint8_t>& data, const std::string& cid);

// ISO-8601 UTC now
std::string now_iso8601();

// Constant-time compare
bool const_time_eq(const std::string& a, const std::string& b);

// JSON helpers
std::string trim(const std::string& s);
bool is_printable_ascii(const std::string& s);

} // namespace utils
} // namespace dwn
