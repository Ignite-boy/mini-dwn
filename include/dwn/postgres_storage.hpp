#pragma once
#include "storage.hpp"
#include <pqxx/pqxx>
#include <mutex>
#include <memory>

namespace dwn {

class PostgresStorage : public Storage {
public:
    explicit PostgresStorage(const std::string& conn_str);
    ~PostgresStorage() override;

    bool health_check() override;
    bool ensure_tenant(const std::string& targetDid) override;

    bool write_record(const Record& record) override;
    std::optional<Record> read_record(const std::string& targetDid, const std::string& recordId) override;
    std::vector<Record> query_records(const std::string& targetDid, const RecordsFilter& filter) override;
    bool delete_record(const std::string& targetDid, const std::string& recordId, bool tombstone) override;

    std::string append_event(const std::string& targetDid, const std::string& recordId, const std::string& eventType, const nlohmann::json& metadata) override;
    std::vector<nlohmann::json> get_events(const std::string& targetDid, int limit) override;

private:
    std::string conn_str_;
    std::unique_ptr<pqxx::connection> conn_;
    std::mutex mu_; // pqxx connection is not thread-safe, guard with mutex for MVP (later use pool)

    pqxx::connection& get_conn();
    void ensure_schema(); // runs migration if needed (for local dev)
};

} // namespace dwn
