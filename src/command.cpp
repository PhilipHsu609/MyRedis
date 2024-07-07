#include "command.hpp"
#include "hashtable.hpp"
#include "utils.hpp"

#include <fmt/core.h> // fmt::print

#include <cstdio>  // stderr
#include <cstring> // std::memcpy
#include <string>
#include <string_view>

namespace {
Response do_unknown(const Request &req, std::byte *res_vec) {
    static constexpr std::string_view msg = "Unknown command";

    Response res{};
    std::memcpy(res_vec, msg.data(), msg.size());

    res.len = msg.size();
    res.status = ResStatus::OK;

    LOG_ERROR(fmt::format("Received unknown command {}", req.args[0]));

    return res;
}

Response do_get(const Request &req, std::byte *res_vec) {
    Response res{};

    const std::string key(req.args[0]);
    if (map.get(key) == nullptr) {
        LOG_INFO(fmt::format("GET Key: {} not found", key));
        res.status = ResStatus::NOT_FOUND;
        res.len = 0;
        return res;
    }

    const std::string &value = map.get(key)->value;
    std::memcpy(res_vec, value.data(), value.size());
    res.status = ResStatus::OK;
    res.len = value.size();

    LOG_INFO(fmt::format("GET Key: {}, Value: {}", key, value));

    return res;
}

Response do_set(const Request &req, std::byte *res_vec) {
    Response res{};

    const std::string key(req.args[0]);
    const std::string value(req.args[1]);

    map.set(key, value);

    std::string msg = fmt::format("Key: {}, Value: {}", key, value);
    std::memcpy(res_vec, msg.data(), msg.size());
    res.status = ResStatus::OK;
    res.len = msg.size();

    LOG_INFO(fmt::format("SET Key: {}, Value: {}", key, value));

    return res;
}

Response do_del(const Request &req, std::byte *res_vec) {
    Response res{};

    const std::string key(req.args[0]);
    if (map.get(key) == nullptr) {
        LOG_INFO(fmt::format("DEL Key: {} not found", key));
        res.status = ResStatus::NOT_FOUND;
        res.len = 0;
        return res;
    }

    map.remove(key);

    std::string msg = fmt::format("Key: {} deleted", key);
    std::memcpy(res_vec, msg.data(), msg.size());
    res.status = ResStatus::OK;
    res.len = msg.size();

    LOG_INFO(fmt::format("DEL Key: {}", key));

    return res;
}
} // namespace

Response do_request(const std::byte *req_vec, std::size_t len, std::byte *res_vec) {
    Request req{};
    Response res{};
    if (parse_request(req_vec, len, req) != 0) {
        res.status = ResStatus::ERR;
        return res;
    }

    switch (req.cmd) {
    case Cmd::GET:
        res = do_get(req, res_vec);
        break;
    case Cmd::SET:
        res = do_set(req, res_vec);
        break;
    case Cmd::DEL:
        res = do_del(req, res_vec);
        break;
    case Cmd::UNKNOWN:
        res = do_unknown(req, res_vec);
        break;
    }

    return res;
}

int parse_request(const std::byte *request, std::size_t len, Request &req) {
    if (len < COMMAND_SIZE) {
        // Not enough data
        return -1;
    }

    std::size_t nstr = 0;
    std::memcpy(&nstr, request, COMMAND_SIZE);

    if (nstr > MAX_ARGS) {
        return -1;
    }

    std::size_t offset = COMMAND_SIZE;

    std::size_t str_len = 0;
    std::memcpy(&str_len, request + offset, COMMAND_SIZE);
    offset += COMMAND_SIZE;

    std::string cmd_str(str_len, '\0');
    std::memcpy(cmd_str.data(), request + offset, str_len);
    offset += str_len;

    if (cmd_str == "GET") {
        req.cmd = Cmd::GET;
    } else if (cmd_str == "SET") {
        req.cmd = Cmd::SET;
    } else if (cmd_str == "DEL") {
        req.cmd = Cmd::DEL;
    } else {
        req.cmd = Cmd::UNKNOWN;
        req.args.emplace_back(to_view(request, offset - str_len, str_len));
    }

    while (--nstr > 0) {
        if (offset + COMMAND_SIZE > len) {
            // Invalid request
            return -1;
        }

        std::memcpy(&str_len, request + offset, COMMAND_SIZE);
        offset += COMMAND_SIZE;

        if (offset + str_len > len) {
            // Invalid request
            return -1;
        }

        req.args.emplace_back(to_view(request, offset, str_len));
        offset += str_len;
    }

    return 0;
}
