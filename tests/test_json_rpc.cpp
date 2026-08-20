
#include <gtest/gtest.h>
#include "dwn/json_rpc.hpp"
#include "dwn/errors.hpp"

using namespace dwn;

TEST(JsonRpc, ParseValid){
    std::string body = R"({"jsonrpc":"2.0","id":"1","method":"dwn.processMessage","params":{"target":"did:example:123","message":{"descriptor":{"interface":"Records","method":"Read","dataFormat":"application/json","dateCreated":"2024-01-01T00:00:00Z","dateModified":"2024-01-01T00:00:00Z","recordId":"abc"},"authorization":{"payload":"e30","signatures":[]}}}})";
    auto req = parse_json_rpc(body, 1024*1024);
    EXPECT_EQ(req.method, "dwn.processMessage");
}

TEST(JsonRpc, InvalidVersion){
    std::string body = R"({"jsonrpc":"1.0","id":"1","method":"test"})";
    EXPECT_THROW(parse_json_rpc(body, 1024*1024), DwnError);
}

TEST(JsonRpc, Oversize){
    std::string body = R"({"jsonrpc":"2.0","id":"1","method":"test"})";
    EXPECT_THROW(parse_json_rpc(body, 10), DwnError);
}
