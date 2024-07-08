#pragma once

#include "utils.hpp"

#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint8_t
#include <memory>      // std::unique_ptr
#include <string>      // std::string
#include <string_view> // std::string_view
#include <vector>      // std::vector

enum class Cmd : std::uint8_t { GET, SET, DEL, NONE };
enum class ResStatus : std::uint8_t { OK, ERR, NOT_FOUND };
enum class ReqStatus : std::uint8_t { OK, ERR, AGAIN };
enum class ConnState : std::uint8_t { REQUEST, RESPONSE, END };

struct Request {
    std::vector<std::string_view> args;
    Cmd cmd = Cmd::NONE;
};

struct Response {
    std::string msg;
    ResStatus status = ResStatus::ERR;
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
    std::unique_ptr<Response> res;

    Connection(int fd) : fd{fd} {
        rbuf.resize(IOBUF_LEN);
        wbuf.resize(IOBUF_LEN);
    }
};

constexpr std::string_view to_string(ResStatus status) {
    switch (status) {
    case ResStatus::OK:
        return "OK";
    case ResStatus::ERR:
        return "ERR";
    case ResStatus::NOT_FOUND:
        return "NOT_FOUND";
    }
}

constexpr std::string_view to_string(Cmd cmd) {
    switch (cmd) {
    case Cmd::GET:
        return "GET";
    case Cmd::SET:
        return "SET";
    case Cmd::DEL:
        return "DEL";
    case Cmd::NONE:
        return "NONE";
    }
}

ReqStatus do_request(std::unique_ptr<Connection> &conn);