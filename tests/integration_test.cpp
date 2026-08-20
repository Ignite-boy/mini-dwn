
#include <gtest/gtest.h>
#include "dwn/records.hpp"
#include "dwn/auth.hpp"
#include "dwn/storage.hpp"
#include "dwn/utils.hpp"

// Integration test mimics the critical flow from spec:
// 1. Start Mini-DWN (mocked)
// 2. Create DID A
// 3. Write record as DID A
// 4. Read as DID A -> success
// 5. Read as DID B -> denied
// 6. Restart server (mock storage persists in memory for this test)
// 7. Read as DID A -> record still exists

class MockStorage : public dwn::Storage {
public:
    std::map<std::string, dwn::Record> store;
    bool health_check() override { return true; }
    bool ensure_tenant(const std::string&) override { return true; }
    bool write_record(const dwn::Record& r) override { store[r.recordId]=r; return true; }
    std::optional<dwn::Record> read_record(const std::string& target, const std::string& id) override {
        auto it=store.find(id);
        if(it==store.end()) return std::nullopt;
        if(it->second.targetDid!=target) return std::nullopt;
        return it->second;
    }
    std::vector<dwn::Record> query_records(const std::string& target, const dwn::RecordsFilter&) override {
        std::vector<dwn::Record> out;
        for(auto& kv: store) if(kv.second.targetDid==target) out.push_back(kv.second);
        return out;
    }
    bool delete_record(const std::string& target, const std::string& id, bool tomb) override {
        auto it=store.find(id);
        if(it==store.end()) return false;
        if(tomb) it->second.deleted=true; else store.erase(it);
        return true;
    }
    std::string append_event(const std::string&, const std::string&, const std::string&, const nlohmann::json&) override { return "evt"; }
    std::vector<nlohmann::json> get_events(const std::string&, int) override { return {}; }
};

TEST(Integration, FullFlow){
    MockStorage storage;
    dwn::AuthVerifier auth;
    dwn::RecordsHandler handler(storage, auth);

    std::string didA = "did:example:alice";
    std::string didB = "did:example:bob";

    // 2. Create DID A (in test, just string)
    // 3. Write record as DID A
    dwn::Message writeMsg;
    writeMsg.targetDid = didA;
    writeMsg.recordId = "test-record-1";
    writeMsg.descriptor.interfaceName="Records";
    writeMsg.descriptor.method="Write";
    writeMsg.descriptor.dataFormat="application/json";
    writeMsg.descriptor.dateCreated=dwn::utils::now_iso8601();
    writeMsg.descriptor.dateModified=writeMsg.descriptor.dateCreated;
    writeMsg.descriptor.recordId="test-record-1";
    std::string payload = R"({"name":"Alice Post"})";
    writeMsg.encodedData = std::vector<uint8_t>(payload.begin(), payload.end());

    auto wRes = handler.handleWrite(writeMsg);
    ASSERT_EQ(wRes.statusCode, 202);

    // 4. Read as DID A -> success
    dwn::Message readA;
    readA.targetDid=didA;
    readA.recordId="test-record-1";
    readA.descriptor.interfaceName="Records";
    readA.descriptor.method="Read";
    readA.descriptor.dataFormat="application/json";
    readA.descriptor.recordId="test-record-1";
    readA.descriptor.dateCreated=dwn::utils::now_iso8601();
    readA.descriptor.dateModified=readA.descriptor.dateCreated;
    auto rA = handler.handleRead(readA);
    EXPECT_EQ(rA.statusCode, 200);

    // 5. Read as DID B -> denied (404 due to tenant isolation)
    dwn::Message readB;
    readB.targetDid=didB;
    readB.recordId="test-record-1";
    readB.descriptor.interfaceName="Records";
    readB.descriptor.method="Read";
    readB.descriptor.dataFormat="application/json";
    readB.descriptor.recordId="test-record-1";
    readB.descriptor.dateCreated=dwn::utils::now_iso8601();
    readB.descriptor.dateModified=readB.descriptor.dateCreated;
    auto rB = handler.handleRead(readB);
    EXPECT_TRUE(rB.statusCode==404 || rB.statusCode==403);

    // 6. Restart server (simulate by new handler using same storage)
    dwn::RecordsHandler handler2(storage, auth);
    // 7. Read as DID A -> record still exists
    auto rA2 = handler2.handleRead(readA);
    EXPECT_EQ(rA2.statusCode, 200);
}
