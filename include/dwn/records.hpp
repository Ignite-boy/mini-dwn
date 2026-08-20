#pragma once
#include "storage.hpp"
#include "auth.hpp"
#include "message.hpp"
#include <nlohmann/json.hpp>

namespace dwn {

struct RecordsResult {
    int statusCode; // e.g. 202, 200, 401, 404
    std::string detail;
    nlohmann::json data; // optional extra payload
};

class RecordsHandler {
public:
    RecordsHandler(Storage& storage, AuthVerifier& auth);

    RecordsResult handleWrite(const Message& msg);
    RecordsResult handleRead(const Message& msg);
    RecordsResult handleQuery(const Message& msg);
    RecordsResult handleDelete(const Message& msg);

private:
    Storage& storage_;
    AuthVerifier& auth_;
};

} // namespace dwn
