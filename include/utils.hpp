#pragma once

#include <cstddef>     // std::byte, std::size_t
#include <cstdint>     // std::uint16_t
#include <cstdio>      // std::FILE
#include <string_view> // std::string_view
#include <vector>      // std::vector

#define FILE_NAME get_filename(__FILE__)
#define LOG_INFO(msg) print_msg(FILE_NAME, __LINE__, __func__, stdout, msg)
#define LOG_ERROR(msg) print_msg(FILE_NAME, __LINE__, __func__, stderr, msg)

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
constexpr std::size_t COMMAND_SIZE = sizeof(std::uint32_t);
constexpr std::size_t MAX_ARGS = 3;
constexpr int MAX_EVENTS = 10;

void set_nonblocking(int fd);

std::int32_t read_all(int fd, std::vector<std::byte> &buf, std::size_t n);
std::int32_t write_all(int fd, const std::vector<std::byte> &buf, std::size_t n);

std::string_view to_view(const std::byte *buf, std::size_t n);
std::string_view to_view(const std::byte *buf, std::size_t offset, std::size_t n);
