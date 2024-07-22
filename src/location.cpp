#include "location.hpp"

#include <fmt/core.h> // fmt::format

std::string Location::to_string() const {
    return fmt::format("{}@{}:{}", func_name_, file_name_, line_number_);
}

#define RETURN_ADDRESS() __builtin_extract_return_addr(__builtin_return_address(0))

Location Location::current(const char *file_name, const char *func_name,
                           int line_number) {
    return {file_name, func_name, line_number, RETURN_ADDRESS()};
}

Location::Location(const char *file_name, const char *func_name, int line_number,
                   const void *program_counter)
    : file_name_(file_name), func_name_(func_name), line_number_(line_number),
      program_counter_(program_counter) {}