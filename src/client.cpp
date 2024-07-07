#include "command.hpp"
#include "utils.hpp"

#include <fmt/core.h> // fmt::print

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
#include <unistd.h>     // close

std::vector<std::byte> make_request(const std::vector<std::string_view> &args) {
    std::size_t len = COMMAND_SIZE;
    for (const auto &arg : args) {
        len += COMMAND_SIZE + arg.size();
    }

    std::vector<std::byte> buf(COMMAND_SIZE + len);
    std::size_t offset = 0;

    std::size_t n = args.size();
    std::memcpy(buf.data(), &len, COMMAND_SIZE);
    offset += COMMAND_SIZE;
    std::memcpy(buf.data() + offset, &n, COMMAND_SIZE);
    offset += COMMAND_SIZE;

    for (const auto &arg : args) {
        n = arg.size();
        std::memcpy(buf.data() + offset, &n, COMMAND_SIZE);
        offset += COMMAND_SIZE;
        std::memcpy(buf.data() + offset, arg.data(), arg.size());
        offset += arg.size();
    }

    return buf;
}

int send_req(int fd, const std::vector<std::byte> &buf) {
    return write_all(fd, buf, buf.size());
}

int read_res(int fd) {
    std::vector<std::byte> buf(COMMAND_SIZE);
    if (read_all(fd, buf, COMMAND_SIZE) != 0) {
        return -1;
    }

    std::size_t len = 0;
    std::memcpy(&len, buf.data(), COMMAND_SIZE);

    buf.resize(len);
    if (read_all(fd, buf, len) != 0) {
        return -1;
    }

    ResStatus status = ResStatus::ERR;
    std::memcpy(&status, buf.data(), sizeof(ResStatus));
    auto msg = to_view(buf.data() + sizeof(ResStatus), len - sizeof(ResStatus));

    fmt::print("[{}] {}\n", to_string(status), msg);

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