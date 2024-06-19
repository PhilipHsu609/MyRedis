#include "utils.hpp"

#include <fmt/ranges.h> // fmt::print

#include <array>       // std::array
#include <cerrno>      // errno
#include <cstddef>     // std::byte, std::size_t
#include <cstdio>      // stderr, BUFSIZ
#include <cstring>     // std::strerror
#include <string>      // std::string
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <netinet/in.h> // sockaddr_in, ntohs
#include <sys/socket.h> // connect, socket
#include <unistd.h>     // close, read, write

std::int32_t send_req(int fd, const std::string &req) {
    const std::size_t len = req.size();
    std::vector<std::byte> buf(BUFSIZ);
    std::memcpy(buf.data(), &len, COMMAND_SIZE);
    std::memcpy(buf.data() + COMMAND_SIZE, req.data(), len);

    return write_all(fd, buf, COMMAND_SIZE + len);
}

std::int32_t recv_res(int fd, std::vector<std::byte> &buf, std::size_t n) {
    return read_all(fd, buf, n);
}

int main() {
    const int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd == -1) {
        fmt::print(stderr, "socket failed: {}\n", std::strerror(errno));
        return 1;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = ntohs(PORT);
    addr.sin_addr.s_addr = ntohl(INADDR_LOOPBACK);

    if (connect(fd, reinterpret_cast<const sockaddr *>(&addr), sizeof(addr)) != 0) {
        fmt::print(stderr, "connect failed: {}\n", std::strerror(errno));
        return 1;
    }

    const std::array<std::string, 3> reqs = {"ping", "pong", "end"};
    for (const auto &req : reqs) {
        send_req(fd, req);
    }

    for (const auto &req : reqs) {
        std::vector<std::byte> buf(BUFSIZ);
        recv_res(fd, buf, COMMAND_SIZE + req.size());
        fmt::print("{}\n",
                   std::string_view{reinterpret_cast<char *>(buf.data() + COMMAND_SIZE),
                                    req.size()});
    }

    close(fd);

    return 0;
}