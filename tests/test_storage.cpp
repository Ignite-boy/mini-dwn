
#include <gtest/gtest.h>
#include "dwn/utils.hpp"

using namespace dwn;

TEST(Utils, Base64Url){
    std::string s="hello world";
    auto enc = utils::base64url_encode(s);
    auto dec = utils::base64url_decode(enc);
    std::string out(dec.begin(), dec.end());
    EXPECT_EQ(out, s);
}

TEST(Utils, Sha256){
    auto h = utils::sha256_hex("test");
    EXPECT_EQ(h, "9f86d081884c7d659a2feaa0c55ad015a3bf4f1b2b0b822cd15d6c15b0f00a08");
}

TEST(Utils, Cid){
    std::vector<uint8_t> data{'a','b','c'};
    auto cid = utils::compute_data_cid(data);
    EXPECT_TRUE(utils::verify_data_cid(data, cid));
    EXPECT_FALSE(utils::verify_data_cid(data, "sha256:deadbeef"));
}
