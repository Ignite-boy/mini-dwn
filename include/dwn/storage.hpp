#pragma once
#include <string>
#include <optional>
#include <vector>
#include "message.hpp"

namespace dwn {

class Storage {
public:
    virtual ~Storage() = default;

    virtual bool health_check() = 0;
    virtual bool ensure_tenant(const std::string& targetDid) = 0;

    virtual bool write_record(const Record& record) = 0;
    virtual std::optional<Record> read_record(const std::string& targetDid, const std::string& recordId) = 0;
    virtual std::vector<Record> query_records(const std::string& targetDid, const RecordsFilter& filter) = 0;
    virtual bool delete_record(const std::string& targetDid, const std::string& recordId, bool tombstone = true) = 0;

    // Events
    virtual std::string append_event(const std::string& targetDid, const std::string& recordId, const std::string& eventType, const nlohmann::json& metadata) = 0;
    virtual std::vector<nlohmann::json> get_events(const std::string& targetDid, int limit = 50) = 0;
};

} // namespace dwn
