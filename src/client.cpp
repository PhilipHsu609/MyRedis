#include "utils.hpp"

#include <fmt/core.h> // fmt::print

#include <cerrno>      // errno
#include <cstddef>     // std::byte, std::size_t
#include <cstring>     // std::strerror
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <netinet/in.h> // sockaddr_in, ntohs
#include <sys/socket.h> // connect, socket
#include <unistd.h>     // close

void print_obj(const std::byte *buf, std::size_t len, char type) {
    if (type == '_') {
        fmt::print("(nil)\n");
        return;
    }

    if (type == ':') {
        long long value = 0;
        std::memcpy(&value, buf, len);
        fmt::print("(integer) {}\n", value);
        return;
    }

    if (type == '$') {
        std::string_view sv{reinterpret_cast<const char *>(buf), len};
        fmt::print("\"{}\"\n", sv);
        return;
    }
}

void print_arr(const std::byte *buf, std::size_t n) {
    for (std::size_t i = 0; i < n; i++) {
        // TODO(_): handle nested arrays

        char type = ' ';
        std::memcpy(&type, buf, 1);
        std::size_t len = 0;
        std::memcpy(&len, &buf[1], CMD_LEN_BYTES);

        print_obj(buf, len, type);

        buf += 1 + CMD_LEN_BYTES;
    }
}

void print_response(const std::byte *buf, std::size_t len) {
    char type = ' ';

    std::memcpy(&type, buf, 1);
    std::memcpy(&len, &buf[1], CMD_LEN_BYTES);

    buf += 1 + CMD_LEN_BYTES;

    if (type == '*') {
        print_arr(buf, len);
        return;
    }

    print_obj(buf, len, type);
}

int send_req(int fd, const std::vector<std::byte> &buf) {
    return write_all(fd, buf, buf.size());
}

int read_res(int fd) {
    std::vector<std::byte> buf(CMD_LEN_BYTES);
    if (read_all(fd, buf, CMD_LEN_BYTES) != 0) {
        return -1;
    }

    std::size_t len = 0;
    std::memcpy(&len, buf.data(), CMD_LEN_BYTES);

    buf.resize(len);
    if (read_all(fd, buf, len) != 0) {
        return -1;
    }

    print_response(buf.data(), len);

    return 0;
}

int main(int argc, char **argv) {
    std::vector<std::string_view> args;

    for (int i = 1; i < argc; i++) {
        args.emplace_back(argv[i]);
    }

    auto req_buf = make_request(args);

    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        LOG_ERROR(fmt::format("socket failed: {}", std::strerror(errno)));
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        LOG_ERROR(fmt::format("connect failed: {}", std::strerror(errno)));
        return 1;
    }

    if (send_req(fd, req_buf) != 0) {
        goto DONE;
    }

    if (read_res(fd) != 0) {
        goto DONE;
    }

DONE:
    close(fd);
    return 0;
}