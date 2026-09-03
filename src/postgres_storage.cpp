
#include "dwn/postgres_storage.hpp"
#include "dwn/utils.hpp"
#include <iostream>

namespace dwn {

PostgresStorage::PostgresStorage(const std::string& conn_str)
    : conn_str_(conn_str) {
    try {
        conn_ = std::make_unique<pqxx::connection>(conn_str_);

        // Persistent JSON snapshots used by Milan's existing REST
        // compatibility layer. Safe to run repeatedly.
        {
            pqxx::work w(*conn_);
            w.exec(R"(
                CREATE TABLE IF NOT EXISTS dwn_database_snapshots (
                    name TEXT PRIMARY KEY,
                    data JSONB NOT NULL,
                    pushed_at TIMESTAMPTZ NOT NULL DEFAULT NOW()
                )
            )");
            w.commit();
        }
    } catch(const std::exception& e){
        std::cerr << "Postgres connection failed: " << e.what() << "\n";
        throw;
    }
}

PostgresStorage::~PostgresStorage() {
    try { if(conn_ && conn_->is_open()) conn_->close(); } catch(...) {}
}

pqxx::connection& PostgresStorage::get_conn(){
    std::lock_guard<std::mutex> lock(mu_);
    if(!conn_ || !conn_->is_open()){
        conn_ = std::make_unique<pqxx::connection>(conn_str_);
    }
    return *conn_;
}

bool PostgresStorage::health_check(){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        w.exec("SELECT 1");
        w.commit();
        return true;
    } catch(...){ return false; }
}

bool PostgresStorage::ensure_tenant(const std::string& targetDid){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        w.exec_params("INSERT INTO dwn_tenants (did, created_at) VALUES ($1, NOW()) ON CONFLICT (did) DO NOTHING", targetDid);
        w.commit();
        return true;
    } catch(const std::exception& e){
        std::cerr << "ensure_tenant error: " << e.what() << "\n";
        return false;
    }
}

bool PostgresStorage::put_database_snapshot(const std::string& name,
                                              const nlohmann::json& data,
                                              const std::string& pushedAt){
    try {
        auto& c = get_conn();
        pqxx::work w(c);

        w.exec_params(
            R"(
                INSERT INTO dwn_database_snapshots (name, data, pushed_at)
                VALUES ($1, $2::jsonb, COALESCE(NULLIF($3, '')::timestamptz, NOW()))
                ON CONFLICT (name) DO UPDATE SET
                    data = EXCLUDED.data,
                    pushed_at = EXCLUDED.pushed_at
            )",
            name,
            data.dump(),
            pushedAt
        );

        w.commit();
        return true;
    } catch(const std::exception& e){
        std::cerr << "put_database_snapshot error: " << e.what() << "\n";
        return false;
    }
}

std::optional<nlohmann::json> PostgresStorage::get_database_snapshot(
    const std::string& name,
    std::string* pushedAt){
    try {
        auto& c = get_conn();
        pqxx::work w(c);

        auto r = w.exec_params(
            "SELECT data::text, pushed_at::text "
            "FROM dwn_database_snapshots WHERE name=$1",
            name
        );

        if(r.empty()){
            w.commit();
            return std::nullopt;
        }

        nlohmann::json data;
        try {
            data = nlohmann::json::parse(r[0][0].as<std::string>());
        } catch(...) {
            data = nlohmann::json::object();
        }

        if(pushedAt) {
            *pushedAt = r[0][1].as<std::string>();
        }

        w.commit();
        return data;
    } catch(const std::exception& e){
        std::cerr << "get_database_snapshot error: " << e.what() << "\n";
        return std::nullopt;
    }
}

bool PostgresStorage::write_record(const Record& record){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        // Upsert record metadata
        w.exec_params(
            R"(
            INSERT INTO dwn_records (record_id, target_did, owner_did, schema, data_format, protocol, protocol_path, recipient, published, date_created, date_modified, deleted, metadata, data_cid, data_size)
            VALUES ($1,$2,$3,$4,$5,$6,$7,$8,$9,$10,$11,$12,$13,$14,$15)
            ON CONFLICT (record_id) DO UPDATE SET
                target_did=EXCLUDED.target_did,
                owner_did=EXCLUDED.owner_did,
                schema=EXCLUDED.schema,
                data_format=EXCLUDED.data_format,
                protocol=EXCLUDED.protocol,
                protocol_path=EXCLUDED.protocol_path,
                recipient=EXCLUDED.recipient,
                published=EXCLUDED.published,
                date_modified=EXCLUDED.date_modified,
                deleted=EXCLUDED.deleted,
                metadata=EXCLUDED.metadata,
                data_cid=EXCLUDED.data_cid,
                data_size=EXCLUDED.data_size
            )",
            record.recordId,
            record.targetDid,
            record.ownerDid,
            record.schema,
            record.dataFormat,
            record.protocol,
            record.protocolPath,
            record.recipient,
            record.published,
            record.dateCreated,
            record.dateModified,
            record.deleted,
            record.metadata.dump(),
            record.dataCid,
            (int64_t)record.dataSize
        );
        // Data table - use bytea
        // pqxx binary handling: use std::string of bytes via escaping
        w.exec_params(
            "INSERT INTO dwn_record_data (record_id, data) VALUES ($1, $2) ON CONFLICT (record_id) DO UPDATE SET data=EXCLUDED.data",
            record.recordId,
            pqxx::binarystring(record.data.data(), record.data.size())
        );
        w.commit();
        return true;
    } catch(const std::exception& e){
        std::cerr << "write_record error: " << e.what() << "\n";
        return false;
    }
}

std::optional<Record> PostgresStorage::read_record(const std::string& targetDid, const std::string& recordId){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        auto r = w.exec_params(
            "SELECT record_id, target_did, owner_did, schema, data_format, protocol, protocol_path, recipient, published, date_created, date_modified, deleted, metadata, data_cid, data_size FROM dwn_records WHERE record_id=$1 AND target_did=$2 AND deleted=false",
            recordId, targetDid
        );
        if(r.empty()) return std::nullopt;
        auto row = r[0];
        Record rec;
        rec.recordId = row[0].as<std::string>();
        rec.targetDid = row[1].as<std::string>();
        rec.ownerDid = row[2].as<std::string>();
        rec.schema = row[3].as<std::string>();
        rec.dataFormat = row[4].as<std::string>();
        rec.protocol = row[5].as<std::string>();
        rec.protocolPath = row[6].as<std::string>();
        rec.recipient = row[7].as<std::string>();
        rec.published = row[8].as<bool>();
        rec.dateCreated = row[9].as<std::string>();
        rec.dateModified = row[10].as<std::string>();
        rec.deleted = row[11].as<bool>();
        try { rec.metadata = nlohmann::json::parse(row[12].as<std::string>()); } catch(...) { rec.metadata = {}; }
        rec.dataCid = row[13].as<std::string>();
        rec.dataSize = row[14].as<int64_t>();

        // Fetch data
        auto dr = w.exec_params("SELECT data FROM dwn_record_data WHERE record_id=$1", recordId);
        if(!dr.empty()){
            auto bin = dr[0][0].as<pqxx::binarystring>();
            rec.data.assign(bin.begin(), bin.end());
        }
        w.commit();
        return rec;
    } catch(const std::exception& e){
        std::cerr << "read_record error: " << e.what() << "\n";
        return std::nullopt;
    }
}

std::vector<Record> PostgresStorage::query_records(const std::string& targetDid, const RecordsFilter& filter){
    std::vector<Record> out;
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        std::string sql = "SELECT record_id, target_did, owner_did, schema, data_format, protocol, protocol_path, recipient, published, date_created, date_modified, deleted, metadata, data_cid, data_size FROM dwn_records WHERE target_did=$1 AND deleted=false";
        std::vector<std::string> params; params.push_back(targetDid);
        int idx=2;
        auto add_cond = [&](const std::string& col, const std::string& val){
            sql += " AND " + col + "=$" + std::to_string(idx++);
            params.push_back(val);
        };
        if(filter.schema) add_cond("schema", *filter.schema);
        if(filter.dataFormat) add_cond("data_format", *filter.dataFormat);
        if(filter.protocol) add_cond("protocol", *filter.protocol);
        if(filter.protocolPath) add_cond("protocol_path", *filter.protocolPath);
        if(filter.recipient) add_cond("recipient", *filter.recipient);
        if(filter.author) add_cond("owner_did", *filter.author);
        if(filter.published){
            sql += " AND published=$" + std::to_string(idx++) ;
            params.push_back(*filter.published ? "true" : "false");
        }
        sql += " ORDER BY date_created DESC";
        if(filter.limit) { sql += " LIMIT " + std::to_string(*filter.limit); }
        if(filter.offset) { sql += " OFFSET " + std::to_string(*filter.offset); }

        // Build pqxx params dynamically - for simplicity use exec_params with up to 8
        // We'll use exec with manual escaping for variable count (MVP) - but use parameterized via transaction exec_params loop?
        // For MVP, use work.exec_params with vector expansion using exec_params via prepared?
        // Simpler: construct query with exec_params using variadic - we will just use exec_params for first param and append others via string (not ideal but for MVP with sanitized inputs from DID validation)
        // NOTE: In production, always use prepared statements. Here targetDid is validated DID, schema etc are validated.
        pqxx::result r;
        if(params.size()==1) r = w.exec_params(sql, params[0]);
        else if(params.size()==2) r = w.exec_params(sql, params[0], params[1]);
        else if(params.size()==3) r = w.exec_params(sql, params[0], params[1], params[2]);
        else if(params.size()==4) r = w.exec_params(sql, params[0], params[1], params[2], params[3]);
        else if(params.size()==5) r = w.exec_params(sql, params[0], params[1], params[2], params[3], params[4]);
        else if(params.size()==6) r = w.exec_params(sql, params[0], params[1], params[2], params[3], params[4], params[5]);
        else r = w.exec_params(sql, params[0]); // fallback

        for(auto row: r){
            Record rec;
            rec.recordId = row[0].as<std::string>();
            rec.targetDid = row[1].as<std::string>();
            rec.ownerDid = row[2].as<std::string>();
            rec.schema = row[3].as<std::string>();
            rec.dataFormat = row[4].as<std::string>();
            rec.protocol = row[5].as<std::string>();
            rec.protocolPath = row[6].as<std::string>();
            rec.recipient = row[7].as<std::string>();
            rec.published = row[8].as<bool>();
            rec.dateCreated = row[9].as<std::string>();
            rec.dateModified = row[10].as<std::string>();
            rec.deleted = row[11].as<bool>();
            try { rec.metadata = nlohmann::json::parse(row[12].as<std::string>()); } catch(...) { rec.metadata = {}; }
            rec.dataCid = row[13].as<std::string>();
            rec.dataSize = row[14].as<int64_t>();
            out.push_back(rec);
        }
        w.commit();
    } catch(const std::exception& e){
        std::cerr << "query_records error: " << e.what() << "\n";
    }
    return out;
}

bool PostgresStorage::delete_record(const std::string& targetDid, const std::string& recordId, bool tombstone){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        if(tombstone){
            w.exec_params("UPDATE dwn_records SET deleted=true, date_modified=NOW() WHERE record_id=$1 AND target_did=$2", recordId, targetDid);
        } else {
            w.exec_params("DELETE FROM dwn_records WHERE record_id=$1 AND target_did=$2", recordId, targetDid);
            w.exec_params("DELETE FROM dwn_record_data WHERE record_id=$1", recordId);
        }
        w.commit();
        return true;
    } catch(const std::exception& e){
        std::cerr << "delete_record error: " << e.what() << "\n";
        return false;
    }
}

std::string PostgresStorage::append_event(const std::string& targetDid, const std::string& recordId, const std::string& eventType, const nlohmann::json& metadata){
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        auto r = w.exec_params("INSERT INTO dwn_events (target_did, record_id, event_type, metadata) VALUES ($1,$2,$3,$4) RETURNING event_id", targetDid, recordId, eventType, metadata.dump());
        std::string eid = r[0][0].as<std::string>();
        w.commit();
        return eid;
    } catch(const std::exception& e){
        std::cerr << "append_event error: " << e.what() << "\n";
        return "";
    }
}

std::vector<nlohmann::json> PostgresStorage::get_events(const std::string& targetDid, int limit){
    std::vector<nlohmann::json> out;
    try {
        auto& c = get_conn();
        pqxx::work w(c);
        auto r = w.exec_params("SELECT event_id, target_did, record_id, event_type, timestamp, metadata FROM dwn_events WHERE target_did=$1 ORDER BY timestamp DESC LIMIT $2", targetDid, limit);
        for(auto row: r){
            nlohmann::json j;
            j["event_id"] = row[0].as<std::string>();
            j["target_did"] = row[1].as<std::string>();
            j["record_id"] = row[2].as<std::string>();
            j["event_type"] = row[3].as<std::string>();
            j["timestamp"] = row[4].as<std::string>();
            try { j["metadata"] = nlohmann::json::parse(row[5].as<std::string>()); } catch(...) { j["metadata"] = {}; }
            out.push_back(j);
        }
        w.commit();
    } catch(...){}
    return out;
}

} // namespace dwn
