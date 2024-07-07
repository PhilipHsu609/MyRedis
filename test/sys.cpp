#include <cstddef> // std::size_t
#include <cstdlib> // std::rand
#include <cstring> // std::strlen, std::memcpy

#include <sys/types.h> // ssize_t
#include <unistd.h>    // read, write

using size_t = std::size_t;

ssize_t read(int fd, void *buf, size_t n) {
    static size_t pos = 0;
    static const char *data = "data in the buffer\n";
    static const size_t data_len = std::strlen(data);

    if (pos == data_len) {
        pos = 0;
        return 0;
    }

    size_t len = n;
    size_t remain = data_len - pos;

    int avail_len = std::rand() % remain + 1;
    if (pos + len > avail_len) {
        len = avail_len;
    }

    std::memcpy(buf, data + pos, avail_len);

    pos += avail_len;
    return avail_len;
}

ssize_t write(int fd, const void *buf, size_t n) { return std::rand() % n + 1; }