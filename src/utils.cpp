#include "utils.hpp"

#include <fmt/ranges.h> // fmt::print

#include <cerrno>      // errno
#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::int32_t
#include <cstdio>      // stderr
#include <cstdlib>     // std::exit
#include <cstring>     // std::strerror
#include <string_view> // std::string_view
#include <vector>      // std::vector

#include <fcntl.h>     // fcntl
#include <sys/types.h> // ssize_t
#include <unistd.h>    // read, write

void print_msg(const char *file, int line, const char *func, std::FILE *f,
               std::string_view msg) {
    fmt::print(f, "[{}:{} {}()]: {}\n", file, line, func, msg);
}

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        LOG_ERROR(fmt::format("fcntl failed: {}", std::strerror(errno)));
        return;
    }

    flags |= O_NONBLOCK;

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
    if (fcntl(fd, F_SETFL, flags) == -1) {
        LOG_ERROR(fmt::format("fcntl failed: {}", std::strerror(errno)));
    }
}

std::int32_t read_all(int fd, std::vector<std::byte> &buf, std::size_t n) {
    auto remain = static_cast<ssize_t>(n);
    ssize_t offset = 0;

    while (remain > 0) {
        const ssize_t m = read(fd, buf.data() + offset, remain);
        if (m <= 0) {
            if (m == -1) {
                LOG_ERROR(fmt::format("read failed: {}", std::strerror(errno)));
            }
            return -1;
        }
        offset += m;
        remain -= m;
    }

    return 0;
}

std::int32_t write_all(int fd, const std::vector<std::byte> &buf, std::size_t n) {
    auto remain = static_cast<ssize_t>(n);
    ssize_t offset = 0;

    while (remain > 0) {
        const ssize_t m = write(fd, buf.data() + offset, remain);
        if (m <= 0) {
            if (m == -1) {
                LOG_ERROR(fmt::format("write failed: {}", std::strerror(errno)));
            }
            return -1;
        }
        offset += m;
        remain -= m;
    }

    return 0;
}

std::string_view to_view(const std::byte *buf, std::size_t n) {
    if (buf == nullptr) {
        return {};
    }
    return std::string_view{reinterpret_cast<const char *>(buf), n};
}

std::string_view to_view(const std::byte *buf, std::size_t offset, std::size_t n) {
    if (buf == nullptr) {
        return {};
    }
    return std::string_view{reinterpret_cast<const char *>(buf + offset), n};
}