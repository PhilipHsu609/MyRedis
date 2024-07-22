find_path(DBG_MACRO_INCLUDE_DIRS "dbg.h")

add_executable(
    server
    server.cpp
    utils.cpp
    command.cpp
    connection.cpp
    hashtable.cpp
    location.cpp
)

add_executable(
    client
    client.cpp
    utils.cpp
    location.cpp
)

set(TARGETS server client)

foreach(TARGET ${TARGETS})
    target_include_directories(
        ${TARGET}
        PRIVATE
        ${PROJECT_SOURCE_DIR}/include
        ${DBG_MACRO_INCLUDE_DIRS})
    target_link_libraries(
        ${TARGET}
        PRIVATE
        fmt::fmt
        compile_flags_interface)
endforeach()