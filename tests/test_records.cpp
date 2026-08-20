
#include <gtest/gtest.h>
#include "dwn/records.hpp"
#include "dwn/auth.hpp"
#include "dwn/storage.hpp"
#include "dwn/utils.hpp"

using namespace dwn;

// Mock storage for unit test
class MockStorage : public Storage {
public:
    std::map<std::string, Record> store;
    bool health_check() override { return true; }
    bool ensure_tenant(const std::string&) override { return true; }
    bool write_record(const Record& r) override { store[r.recordId]=r; return true; }
    std::optional<Record> read_record(const std::string& target, const std::string& id) override {
        auto it=store.find(id);
        if(it==store.end()) return std::nullopt;
        if(it->second.targetDid!=target) return std::nullopt;
        return it->second;
    }
    std::vector<Record> query_records(const std::string& target, const RecordsFilter& f) override {
        std::vector<Record> out;
        for(auto& kv: store){
            if(kv.second.targetDid==target && !kv.second.deleted) out.push_back(kv.second);
        }
        return out;
    }
    bool delete_record(const std::string& target, const std::string& id, bool tomb) override {
        auto it=store.find(id);
        if(it==store.end()) return false;
        if(tomb) it->second.deleted=true;
        else store.erase(it);
        return true;
    }
    std::string append_event(const std::string&, const std::string&, const std::string&, const nlohmann::json&) override { return "evt"; }
    std::vector<nlohmann::json> get_events(const std::string&, int) override { return {}; }
};

TEST(Records, WriteRead){
    MockStorage storage;
    AuthVerifier auth;
    RecordsHandler handler(storage, auth);

    // Build a write message as did:example (dev mode allows unsigned)
    Message msg;
    msg.targetDid = "did:example:123";
    msg.recordId = "rec-1";
    msg.descriptor.interfaceName="Records";
    msg.descriptor.method="Write";
    msg.descriptor.dataFormat="application/json";
    msg.descriptor.dateCreated=utils::now_iso8601();
    msg.descriptor.dateModified=msg.descriptor.dateCreated;
    msg.descriptor.recordId="rec-1";
    std::string jsonData = R"({"hello":"world"})";
    msg.encodedData = std::vector<uint8_t>(jsonData.begin(), jsonData.end());

    auto res = handler.handleWrite(msg);
    EXPECT_EQ(res.statusCode, 202);

    // Now read
    Message readMsg;
    readMsg.targetDid="did:example:123";
    readMsg.recordId="rec-1";
    readMsg.descriptor.interfaceName="Records";
    readMsg.descriptor.method="Read";
    readMsg.descriptor.dataFormat="application/json";
    readMsg.descriptor.recordId="rec-1";
    readMsg.descriptor.dateCreated=utils::now_iso8601();
    readMsg.descriptor.dateModified=readMsg.descriptor.dateCreated;

    auto res2 = handler.handleRead(readMsg);
    EXPECT_EQ(res2.statusCode, 200);
}

TEST(Records, TenantIsolation){
    MockStorage storage;
    AuthVerifier auth;
    RecordsHandler handler(storage, auth);

    Message msg;
    msg.targetDid="did:example:123";
    msg.recordId="rec-iso";
    msg.descriptor.interfaceName="Records";
    msg.descriptor.method="Write";
    msg.descriptor.dataFormat="text/plain";
    msg.descriptor.dateCreated=utils::now_iso8601();
    msg.descriptor.dateModified=msg.descriptor.dateCreated;
    msg.descriptor.recordId="rec-iso";
    std::string data="secret";
    msg.encodedData=std::vector<uint8_t>(data.begin(), data.end());
    handler.handleWrite(msg);

    // Try read as different DID - should be denied because storage read will fail (different tenant) but auth also fails
    Message readB;
    readB.targetDid="did:example:999";
    readB.recordId="rec-iso";
    readB.descriptor.interfaceName="Records";
    readB.descriptor.method="Read";
    readB.descriptor.dataFormat="text/plain";
    readB.descriptor.recordId="rec-iso";
    readB.descriptor.dateCreated=utils::now_iso8601();
    readB.descriptor.dateModified=readB.descriptor.dateCreated;

    auto res = handler.handleRead(readB);
    // In mock, storage isolates by targetDid, so read will not find record -> 404, which is acceptable for isolation
    EXPECT_TRUE(res.statusCode==404 || res.statusCode==403);
}
