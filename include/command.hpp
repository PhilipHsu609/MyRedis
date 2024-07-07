#pragma once

#include <cstddef> // std::size_t, std::byte
#include <cstdint> // std::uint8_t
#include <string_view>
#include <vector>

enum class Cmd : std::uint8_t { GET = 0, SET = 1, DEL = 2, UNKNOWN = 255 };
enum class ResStatus : std::uint8_t { OK = 0, ERR = 1, NOT_FOUND = 2 };

struct Request {
    std::vector<std::string_view> args;
    Cmd cmd = Cmd::UNKNOWN;
};

struct Response {
    std::uint32_t len = 0; // Response length, including status and data
    ResStatus status = ResStatus::ERR;
};

int parse_request(const std::byte *request, std::size_t len, Request &req);
Response do_request(const std::byte *req_vec, std::size_t len, std::byte *res_vec);

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
    case Cmd::UNKNOWN:
        return "UNKNOWN";
    }
}