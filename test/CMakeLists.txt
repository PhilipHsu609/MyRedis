set(TEST_TARGET mytest)

add_executable(
    ${TEST_TARGET}
    test.cpp
)

target_include_directories(
    ${TEST_TARGET}
    PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

target_link_libraries(
    ${TEST_TARGET}
    PRIVATE
    fmt::fmt
    GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(${TEST_TARGET})