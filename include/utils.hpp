#pragma once

#include <cstddef> // std::byte, std::size_t
#include <cstdint> // std::uint16_t
#include <vector>  // std::vector

constexpr std::uint16_t PORT = 1234;
constexpr std::size_t COMMAND_SIZE = 4;
constexpr int MAX_EVENTS = 10;

void set_nonblocking(int fd);
std::int32_t read_all(int fd, std::vector<std::byte> &buf, std::size_t n);
std::int32_t write_all(int fd, const std::vector<std::byte> &buf, std::size_t n);