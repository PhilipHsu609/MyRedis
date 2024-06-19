find_path(DBG_MACRO_INCLUDE_DIRS "dbg.h")

# file(GLOB SOURCE_FILES ${CMAKE_CURRENT_LIST_DIR}/*.cpp)
set(SOURCE_FILES
    ${CMAKE_CURRENT_LIST_DIR}/server.cpp
    ${CMAKE_CURRENT_LIST_DIR}/client.cpp
)

foreach(SOURCE_FILE ${SOURCE_FILES})
    get_filename_component(EXECUTABLE_NAME ${SOURCE_FILE} NAME_WE)
    add_executable(${EXECUTABLE_NAME})
    target_sources(${EXECUTABLE_NAME} PRIVATE ${SOURCE_FILE} utils.cpp)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${PROJECT_SOURCE_DIR}/include)
    target_include_directories(${EXECUTABLE_NAME} PRIVATE ${DBG_MACRO_INCLUDE_DIRS})
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE fmt::fmt)
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE compile_flags_interface)
endforeach()