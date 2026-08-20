
#include "dwn/utils.hpp"
#include <openssl/sha.h>
#include <openssl/evp.h>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <cctype>
#include <cstring>

namespace dwn::utils {

std::string base64url_encode(const std::vector<uint8_t>& data){
    if(data.empty()) return "";
    std::string b64;
    b64.resize(4 * ((data.size()+2)/3));
    int len = EVP_EncodeBlock(reinterpret_cast<unsigned char*>(b64.data()), data.data(), data.size());
    b64.resize(len);
    for(char& ch: b64){
        if(ch=='+') ch='-';
        else if(ch=='/') ch='_';
    }
    while(!b64.empty() && b64.back()=='=') b64.pop_back();
    return b64;
}
std::string base64url_encode(const std::string& s){
    return base64url_encode(std::vector<uint8_t>(s.begin(), s.end()));
}

std::vector<uint8_t> base64url_decode(const std::string& s){
    if(s.empty()) return {};
    std::string b64 = s;
    for(char& ch: b64){
        if(ch=='-') ch='+';
        else if(ch=='_') ch='/';
    }
    size_t mod = b64.size() % 4;
    if(mod) b64.append(4-mod, '=');
    std::vector<uint8_t> out(b64.size());
    int len = EVP_DecodeBlock(out.data(), reinterpret_cast<const unsigned char*>(b64.data()), b64.size());
    if(len < 0) throw std::runtime_error("base64url decode failed");
    size_t eq = 0;
    for(auto it = b64.rbegin(); it!=b64.rend() && *it=='='; ++it) eq++;
    out.resize(len - eq);
    return out;
}

bool is_valid_base64url(const std::string& s){
    if(s.empty()) return true;
    for(char c: s){
        if(!( (c>='A'&&c<='Z') || (c>='a'&&c<='z') || (c>='0'&&c<='9') || c=='-' || c=='_' )) return false;
    }
    return true;
}

static const char* B58_CHARS = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

std::vector<uint8_t> base58_decode(const std::string& b58){
    std::vector<uint8_t> result; result.push_back(0);
    for(char ch: b58){
        const char* pos = strchr(B58_CHARS, ch);
        if(!pos) throw std::runtime_error("invalid base58 char");
        int carry = pos - B58_CHARS;
        for(auto it = result.rbegin(); it!=result.rend(); ++it){
            carry += 58 * (*it);
            *it = carry & 0xFF;
            carry >>= 8;
        }
        while(carry){ result.insert(result.begin(), carry & 0xFF); carry >>= 8; }
    }
    size_t leading = 0;
    for(char ch: b58){ if(ch=='1') leading++; else break; }
    size_t i=0;
    while(i<result.size() && result[i]==0) i++;
    std::vector<uint8_t> trimmed;
    trimmed.reserve(leading + (result.size()-i));
    trimmed.insert(trimmed.end(), leading, 0);
    trimmed.insert(trimmed.end(), result.begin()+i, result.end());
    return trimmed;
}

std::string base58_encode(const std::vector<uint8_t>& data){
    size_t zeros=0;
    while(zeros<data.size() && data[zeros]==0) zeros++;
    std::vector<uint8_t> b58;
    std::vector<uint8_t> input(data.begin()+zeros, data.end());
    while(!input.empty()){
        int carry=0;
        std::vector<uint8_t> next; next.reserve(input.size());
        for(uint8_t byte: input){
            int cur = (carry<<8) + byte;
            int q = cur / 58;
            carry = cur % 58;
            if(!next.empty() || q!=0) next.push_back(q);
        }
        b58.push_back(carry);
        input.swap(next);
    }
    std::string out; out.append(zeros, '1');
    for(auto it=b58.rbegin(); it!=b58.rend(); ++it) out.push_back(B58_CHARS[*it]);
    if(out.empty()) out="1";
    return out;
}

std::array<uint8_t,32> sha256(const std::vector<uint8_t>& data){
    std::array<uint8_t,32> out;
    SHA256_CTX ctx; SHA256_Init(&ctx); SHA256_Update(&ctx, data.data(), data.size()); SHA256_Final(out.data(), &ctx);
    return out;
}
std::array<uint8_t,32> sha256(const std::string& s){ return sha256(std::vector<uint8_t>(s.begin(), s.end())); }

std::string sha256_hex(const std::string& s){ auto h=sha256(s); std::ostringstream oss; for(auto b:h) oss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)b; return oss.str(); }
std::string sha256_hex(const std::vector<uint8_t>& data){ auto h=sha256(data); std::ostringstream oss; for(auto b:h) oss<<std::hex<<std::setw(2)<<std::setfill('0')<<(int)b; return oss.str(); }

std::string compute_data_cid(const std::vector<uint8_t>& data){
    return "sha256:" + sha256_hex(data);
}
bool verify_data_cid(const std::vector<uint8_t>& data, const std::string& cid){
    if(cid.rfind("sha256:",0)==0) return cid.substr(7)==sha256_hex(data);
    return cid==sha256_hex(data);
}

std::string now_iso8601(){
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm tm; gmtime_r(&t,&tm);
    char buf[32]; strftime(buf,sizeof(buf),"%Y-%m-%dT%H:%M:%SZ",&tm);
    return std::string(buf);
}

bool const_time_eq(const std::string& a, const std::string& b){
    if(a.size()!=b.size()) return false;
    volatile int diff=0;
    for(size_t i=0;i<a.size();++i) diff |= a[i]^b[i];
    return diff==0;
}

std::string trim(const std::string& s){
    size_t a=0; while(a<s.size() && isspace((unsigned char)s[a])) a++;
    size_t b=s.size(); while(b>a && isspace((unsigned char)s[b-1])) b--;
    return s.substr(a,b-a);
}
bool is_printable_ascii(const std::string& s){
    for(unsigned char c: s) if(c<32 || c>126) return false;
    return true;
}

} // namespace dwn::utils
