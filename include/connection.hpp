#pragma once

#include "utils.hpp"

#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint8_t
#include <memory>      // std::unique_ptr
#include <string_view> // std::string_view
#include <vector>      // std::vector

enum class Cmd : std::uint8_t { GET, SET, DEL, KEYS, NONE };

enum class ReqStatus : std::uint8_t { OK, ERR, AGAIN };
enum class ConnState : std::uint8_t { REQUEST, RESPONSE, END };
enum class ObjType : std::uint8_t { NIL = '_', INT = ':', STR = '$', ARR = '*' };

struct Request {
    // A view of Connection::rbuf
    std::vector<std::string_view> args;
    Cmd cmd = Cmd::NONE;
};

struct Connection {
    int fd = -1;
    // Current state of the connection
    ConnState state = ConnState::REQUEST;
    // Reading
    std::size_t rbuf_size = 0;   // Size of the piped requests in rbuf
    std::size_t rbuf_pos = 0;    // Current position in rbuf
    std::vector<std::byte> rbuf; // [rbuf_pos, rbuf_size) are the requests to be processed
    // Writing
    std::size_t wbuf_size = 0;   // Size of the piped responses in wbuf
    std::size_t wbuf_pos = 0;    // Current position in wbuf
    std::vector<std::byte> wbuf; // [wbuf_pos, wbuf_size) are the responses to be sent

    std::unique_ptr<Request> req;

    Connection(int fd) : fd{fd} {
        rbuf.resize(IOBUF_LEN);
        wbuf.resize(IOBUF_LEN);
    }
};

constexpr std::string_view to_string(Cmd cmd) {
    switch (cmd) {
    case Cmd::GET:
        return "GET";
    case Cmd::SET:
        return "SET";
    case Cmd::DEL:
        return "DEL";
    case Cmd::KEYS:
        return "KEYS";
    case Cmd::NONE:
        return "NONE";
    }
}

ReqStatus do_request(std::unique_ptr<Connection> &conn);

void add_reply(std::unique_ptr<Connection> &conn, const std::vector<std::byte> &msg,
               ObjType type = ObjType::STR);
void add_reply_raw(std::unique_ptr<Connection> &conn, const std::vector<std::byte> &msg,
                   ObjType type = ObjType::STR);