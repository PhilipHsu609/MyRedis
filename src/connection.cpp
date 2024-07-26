#include "connection.hpp"
#include "command.hpp"
#include "utils.hpp"

#include <fmt/core.h> // fmt::format

#include <cstddef> // std::size_t
#include <cstring> // std::memcpy
#include <memory>  // std::unique_ptr

namespace {
ReqStatus parse_request(std::unique_ptr<Connection> &conn) {
    std::size_t nstr = 0;
    std::memcpy(&nstr, &conn->rbuf[conn->rbuf_pos], CMD_LEN_BYTES);
    conn->rbuf_pos += CMD_LEN_BYTES;

    conn->req = std::make_unique<Request>();

    for (std::size_t i = 0; i < nstr; ++i) {
        std::size_t str_len = 0;
        std::memcpy(&str_len, &conn->rbuf[conn->rbuf_pos], CMD_LEN_BYTES);
        conn->rbuf_pos += CMD_LEN_BYTES;

        if (conn->rbuf_size < conn->rbuf_pos + str_len) {
            LOG_ERROR(fmt::format(
                "Parse error: fd = {} at {}-th string. Expected length: {}, got: {}",
                conn->fd, i, str_len, conn->rbuf_size - conn->rbuf_pos));
            return ReqStatus::ERR;
        }

        conn->req->args.emplace_back(to_view(conn->rbuf, conn->rbuf_pos, str_len));
        conn->rbuf_pos += str_len;
    }

    const std::string_view cmd_str{conn->req->args[0]};

    if (cmd_str == "GET") {
        conn->req->cmd = Cmd::GET;
    } else if (cmd_str == "SET") {
        conn->req->cmd = Cmd::SET;
    } else if (cmd_str == "DEL") {
        conn->req->cmd = Cmd::DEL;
    } else {
        conn->req->cmd = Cmd::NONE;
    }

    return ReqStatus::OK;
}
} // namespace

ReqStatus do_request(std::unique_ptr<Connection> &conn) {
    if (conn->rbuf_size < conn->rbuf_pos + CMD_LEN_BYTES) {
        return ReqStatus::AGAIN;
    }

    std::size_t len = 0;
    std::memcpy(&len, &conn->rbuf[conn->rbuf_pos], CMD_LEN_BYTES);
    conn->rbuf_pos += CMD_LEN_BYTES;

    if (len > IOBUF_LEN) {
        LOG_ERROR(fmt::format("Invalid length: {}", len));
        return ReqStatus::ERR;
    }

    if (conn->rbuf_size < conn->rbuf_pos + len) {
        conn->rbuf_pos -= CMD_LEN_BYTES;
        return ReqStatus::AGAIN;
    }

    LOG_INFO(fmt::format("Received: fd = {}, len = {}", conn->fd, len));

    if (parse_request(conn) == ReqStatus::ERR) {
        return ReqStatus::ERR;
    }

    switch (conn->req->cmd) {
    case Cmd::GET:
        do_get(conn);
        break;
    case Cmd::SET:
        do_set(conn);
        break;
    case Cmd::DEL:
        do_del(conn);
        break;
    case Cmd::NONE:
        do_unknown(conn);
        break;
    }

    return ReqStatus::OK;
}

void add_reply(std::unique_ptr<Connection> &conn, const std::vector<std::byte> &msg,
               ObjType type) {
    const std::size_t len = sizeof(ObjType) + CMD_LEN_BYTES + msg.size();

    // TODO(_): Handle the case when wbuf is full

    // Protocol header
    std::memcpy(&conn->wbuf[conn->wbuf_size], &len, CMD_LEN_BYTES);
    conn->wbuf_size += CMD_LEN_BYTES;

    // Protocol body
    add_reply_raw(conn, msg, type);
}

void add_reply_raw(std::unique_ptr<Connection> &conn, const std::vector<std::byte> &msg,
                   ObjType type) {
    const std::size_t msg_len = msg.size();

    // Response header
    std::memcpy(&conn->wbuf[conn->wbuf_size], &type, sizeof(ObjType));
    conn->wbuf_size += sizeof(ObjType);

    // Response body
    std::memcpy(&conn->wbuf[conn->wbuf_size], &msg_len, CMD_LEN_BYTES);
    conn->wbuf_size += CMD_LEN_BYTES;

    std::memcpy(&conn->wbuf[conn->wbuf_size], msg.data(), msg_len);
    conn->wbuf_size += msg_len;
}
