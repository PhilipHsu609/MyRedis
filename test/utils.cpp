#include "utils.hpp"

#include <fmt/core.h>
#include <gtest/gtest.h>

#include <cstddef>     // std::byte
#include <string>      // std::string
#include <string_view> // std::string_view

TEST(Utils, GetFilename) {
    const char *file = "/path/to/file.cpp";
    const char *filename = get_filename(file);
    EXPECT_STREQ(filename, "file.cpp");
}

TEST(Utils, ToView) {
    std::vector<std::byte> buf = {std::byte{'y'}, std::byte{'e'}, std::byte{'s'}};
    std::string_view view = to_view(buf.data(), buf.size());
    EXPECT_EQ(view.size(), buf.size());
    EXPECT_EQ(view[0], 'y');
    EXPECT_EQ(view[1], 'e');
    EXPECT_EQ(view[2], 's');
}

TEST(Utils, ReadAll) {
    int fd = -1; // make sure not calling read(2)
    std::vector<std::byte> buf(32);
    std::size_t n = buf.size();

    auto ret = read_all(fd, buf, n);
    std::string msg{to_view(buf.data(), n)};

    EXPECT_STREQ(msg.c_str(), "data in the buffer\n");
    EXPECT_EQ(ret, -1);
}

TEST(Utils, WriteAll) {
    int fd = -1; // make sure not calling write(2)
    std::vector<std::byte> buf(32);
    std::size_t n = buf.size();

    auto ret = write_all(fd, buf, n);
    EXPECT_EQ(ret, 0);
}