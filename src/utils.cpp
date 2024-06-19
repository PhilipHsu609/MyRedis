#include "utils.hpp"

#include <fmt/ranges.h> // fmt::print

#include <cerrno>  // errno
#include <cstddef> // std::byte, std::size_t
#include <cstdint> // std::int32_t
#include <cstdio>  // stderr
#include <cstring> // std::strerror
#include <vector>  // std::vector

#include <fcntl.h>     // fcntl
#include <sys/types.h> // ssize_t
#include <unistd.h>    // read, write

void set_nonblocking(int fd) {
    errno = 0;
    int flags = fcntl(fd, F_GETFL, 0);
    if (errno != 0) {
        fmt::print(stderr, "fcntl failed: {}\n", std::strerror(errno));
        return;
    }

    flags |= O_NONBLOCK;

    errno = 0;
    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-vararg, hicpp-vararg)
    fcntl(fd, F_SETFL, flags);
    if (errno != 0) {
        fmt::print(stderr, "fcntl failed: {}\n", std::strerror(errno));
    }
}

std::int32_t read_all(int fd, std::vector<std::byte> &buf, std::size_t n) {
    auto remain = static_cast<ssize_t>(n);
    ssize_t offset = 0;

    while (remain > 0) {
        const ssize_t m = read(fd, buf.data() + offset, remain);
        if (m <= 0) {
            fmt::print(stderr, "read failed: {}\n", std::strerror(errno));
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
            fmt::print(stderr, "write failed: {}\n", std::strerror(errno));
            return -1;
        }
        offset += m;
        remain -= m;
    }

    return 0;
}
