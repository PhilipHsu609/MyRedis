cmake_minimum_required(VERSION 3.15)

set(CMAKE_TOOLCHAIN_FILE ~/package/vcpkg/scripts/buildsystems/vcpkg.cmake CACHE STRING "Vcpkg toolchain file")

project(
    MyRedis-Test
    VERSION 1.0
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED on)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${PROJECT_SOURCE_DIR}/../bin/test)

# Add your source files here
set(SOURCES
    test.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../src/utils.cpp
)

# vcpkg dependencies
find_package(fmt CONFIG REQUIRED)
find_package(GTest CONFIG REQUIRED)

enable_testing()

add_executable(
    ${PROJECT_NAME}
    ${SOURCES}
)

target_include_directories(
    ${PROJECT_NAME}
    PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../include
)

target_link_libraries(
    ${PROJECT_NAME}
    PRIVATE
    fmt::fmt
    GTest::gtest
)

include(GoogleTest)
gtest_discover_tests(${PROJECT_NAME})