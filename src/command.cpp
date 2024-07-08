#include "command.hpp"
#include "connection.hpp"
#include "hashtable.hpp"
#include "utils.hpp"

#include <fmt/core.h> // fmt::format

#include <cstring>     // std::memcpy
#include <memory>      // std::unique_ptr
#include <string>      // std::string
#include <string_view> // std::string_view

void do_unknown(std::unique_ptr<Connection> &conn) {
    static constexpr std::string_view msg = "Unknown command";

    conn->res = std::make_unique<Response>();
    conn->res->msg = std::string(msg);

    conn->res->status = ResStatus::OK;

    LOG_ERROR(fmt::format("Received unknown command {}", conn->req->args[0]));
}

void do_get(std::unique_ptr<Connection> &conn) {
    conn->res = std::make_unique<Response>();

    const std::string key(conn->req->args[1]);
    if (map.get(key) == nullptr) {
        LOG_INFO(fmt::format("GET Key: {} not found", key));
        conn->res->status = ResStatus::NOT_FOUND;
        return;
    }

    const std::string &value = map.get(key)->value;
    conn->res->msg = std::string(value);
    conn->res->status = ResStatus::OK;

    LOG_INFO(fmt::format("GET Key: {}, Value: {}", key, value));
}

void do_set(std::unique_ptr<Connection> &conn) {
    conn->res = std::make_unique<Response>();

    const std::string key(conn->req->args[1]);
    const std::string value(conn->req->args[2]);

    map.set(key, value);

    conn->res->msg = fmt::format("Key: {}, Value: {}", key, value);
    conn->res->status = ResStatus::OK;

    LOG_INFO(fmt::format("SET Key: {}, Value: {}", key, value));
}

void do_del(std::unique_ptr<Connection> &conn) {
    conn->res = std::make_unique<Response>();

    const std::string key(conn->req->args[1]);
    if (map.get(key) == nullptr) {
        LOG_INFO(fmt::format("DEL Key: {} not found", key));
        conn->res->status = ResStatus::NOT_FOUND;
        return;
    }

    map.remove(key);

    conn->res->msg = fmt::format("Key: {} deleted", key);
    conn->res->status = ResStatus::OK;

    LOG_INFO(fmt::format("DEL Key: {}", key));
}