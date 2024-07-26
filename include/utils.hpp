#pragma once

#include "location.hpp"

#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint16_t
#include <cstdio>      // std::FILE
#include <string_view> // std::string_view
#include <vector>      // std::vector

// #define FILE_NAME get_filename(__FILE__)
// #define LOG_INFO(msg) print_msg(FILE_NAME, __LINE__, __func__, stdout, msg)
// #define LOG_ERROR(msg) print_msg(FILE_NAME, __LINE__, __func__, stderr, msg)

constexpr const char *get_filename(const char *file) {
    const char *p = file;
    while (*p != '\0') {
        if (*p == '/') {
            file = p + 1;
        }
        ++p;
    }
    return file;
}

void print_msg(const char *file, int line, const char *func, std::FILE *f,
               std::string_view msg);

constexpr std::uint16_t PORT = 1234;
constexpr std::size_t IOBUF_LEN = 8UL * 1024UL;
constexpr std::size_t CMD_LEN_BYTES = sizeof(std::uint32_t);
constexpr std::size_t MAX_ARGS = 3;
constexpr int MAX_EVENTS = 10;

void set_nonblocking(int fd);

std::int32_t read_all(int fd, std::vector<std::byte> &buf, std::size_t n);
std::int32_t write_all(int fd, const std::vector<std::byte> &buf, std::size_t n);

std::string_view to_view(const std::vector<std::byte> &buf, std::size_t n);
std::string_view to_view(const std::vector<std::byte> &buf, std::size_t offset,
                         std::size_t n);
std::vector<std::byte> to_bytes(std::string_view sv);

std::vector<std::byte> make_request(const std::vector<std::string_view> &args);

#define CURRENT_LOCATION Location::current()
#define LOG_DEBUG(msg) Logger::debug((msg), CURRENT_LOCATION)
#define LOG_INFO(msg) Logger::info((msg), CURRENT_LOCATION)
#define LOG_WARNING(msg) Logger::warning((msg), CURRENT_LOCATION)
#define LOG_ERROR(msg) Logger::error((msg), CURRENT_LOCATION)

namespace Logger {
enum class Level : std::uint8_t {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3,
    DISABLED = 4
};

void set_level(Level level);

void log_write(Level level, const std::string &msg, const Location &loc);
inline void debug(const std::string &msg, const Location &loc) {
    log_write(Level::DEBUG, msg, loc);
}
inline void info(const std::string &msg, const Location &loc) {
    log_write(Level::INFO, msg, loc);
}
inline void warning(const std::string &msg, const Location &loc) {
    log_write(Level::WARNING, msg, loc);
}
inline void error(const std::string &msg, const Location &loc) {
    log_write(Level::ERROR, msg, loc);
}
} // namespace Logger
