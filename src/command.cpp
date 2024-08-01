#include "command.hpp"
#include "connection.hpp"
#include "hashtable.hpp"
#include "utils.hpp"

#include <cstddef>
#include <fmt/core.h> // fmt::format

#include <cstring>     // std::memcpy
#include <memory>      // std::unique_ptr
#include <string>      // std::string
#include <string_view> // std::string_view
#include <vector>

void do_unknown(std::unique_ptr<Connection> &conn) {
    static constexpr std::string_view msg = "Unknown command";
    add_reply(conn, to_bytes(msg));
    LOG_ERROR(fmt::format("Received unknown command {}", conn->req->args[0]));
}

void do_get(std::unique_ptr<Connection> &conn) {
    const std::string key(conn->req->args[1]);
    if (map.get(key) == nullptr) {
        add_reply(conn, {}, ObjType::NIL);
        return;
    }

    std::string_view value = map.get(key)->value;

    LOG_INFO(fmt::format("GET Key: {}, Value: {}", key, value));

    add_reply(conn, to_bytes(value));
}

void do_set(std::unique_ptr<Connection> &conn) {
    const std::string key(conn->req->args[1]);
    const std::string value(conn->req->args[2]);

    map.set(key, value);

    LOG_INFO(fmt::format("SET Key: {}, Value: {}", key, value));

    add_reply(conn, to_bytes("OK"));
}

void do_del(std::unique_ptr<Connection> &conn) {
    const std::string key(conn->req->args[1]);
    if (map.get(key) == nullptr) {
        std::vector<std::byte> buf{std::byte(0)};
        add_reply(conn, buf, ObjType::INT);
        return;
    }

    map.remove(key);

    LOG_INFO(fmt::format("DEL Key: {}", key));

    std::vector<std::byte> buf{std::byte(1)};
    add_reply(conn, buf, ObjType::INT);
}

void do_keys(std::unique_ptr<Connection> &conn) {
    if (map.is_empty()) {
        add_reply(conn, {}, ObjType::NIL);
        return;
    }

    const std::vector<std::string> keys = map.keys();

    LOG_INFO(fmt::format("KEYS: {}...", keys[0]));

    auto *reserved = &conn->wbuf[conn->wbuf_size];
    // Reserve space for the protocol header and the response header
    conn->wbuf_size += CMD_LEN_BYTES + sizeof(ObjType) + CMD_LEN_BYTES;

    std::size_t len = 0;
    for (const auto &key : keys) {
        add_reply_raw(conn, to_bytes(key));
        len += key.size();
        LOG_DEBUG(fmt::format("key: {}, len: {}", key, len));
    }

    const ObjType type = ObjType::ARR;
    const std::size_t nelems = keys.size();
    len += (sizeof(ObjType) + CMD_LEN_BYTES) * (nelems + 1);

    LOG_DEBUG(fmt::format("KEYS: len = {}, nelems = {}", len, nelems));

    // Protocol header
    std::memcpy(reserved, &len, CMD_LEN_BYTES);
    // Response header
    std::memcpy(reserved + CMD_LEN_BYTES, &type, sizeof(ObjType));
    std::memcpy(reserved + CMD_LEN_BYTES + sizeof(ObjType), &nelems, CMD_LEN_BYTES);
}