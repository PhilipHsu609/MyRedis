/*
    ref: https://source.chromium.org/chromium/chromium/src/+/main:base/location.h
*/
#pragma once

#include <string> // std::string

class Location {
  public:
    Location() = default;

    const char *file_name() const { return file_name_; }
    const char *func_name() const { return func_name_; }
    int line_number() const { return line_number_; }
    const void *program_counter() const { return program_counter_; }

    std::string to_string() const;

    // https://clang.llvm.org/docs/LanguageExtensions.html#source-location-builtins
    static Location current(const char *file_name = __builtin_FILE_NAME(),
                            const char *func_name = __builtin_FUNCTION(),
                            int line_number = __builtin_LINE());

  private:
    Location(const char *file_name, const char *func_name, int line_number,
             const void *program_counter);

    const char *file_name_ = nullptr;
    const char *func_name_ = nullptr;
    int line_number_ = -1;
    const void *program_counter_ = nullptr;
};