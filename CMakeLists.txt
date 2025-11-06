cmake_minimum_required(VERSION 3.16)
project(Tez)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
find_package(GTest QUIET)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)

# Main executable with all source files
add_executable(Tez
    src/main.cpp
    src/router.cpp
    src/middleware.cpp
    src/file_server.cpp
    src/thread_pool.cpp
    src/request.cpp
)

target_link_libraries(Tez ${Boost_LIBRARIES})
target_link_libraries(Tez nlohmann_json::nlohmann_json)

# Tests (only if GTest is available)
if(GTest_FOUND)
    enable_testing()

    # Create a library for testable components
    add_library(TezLib STATIC
        src/router.cpp
        src/middleware.cpp
        src/file_server.cpp
        src/thread_pool.cpp
        src/request.cpp
    )
    target_link_libraries(TezLib ${Boost_LIBRARIES} nlohmann_json::nlohmann_json)

    # Test executable
    add_executable(TezTests
        tests/test_router.cpp
        tests/test_middleware.cpp
        tests/test_file_server.cpp
        tests/test_response.cpp
    )

    target_link_libraries(TezTests
        TezLib
        GTest::GTest
        GTest::Main
        ${Boost_LIBRARIES}
        nlohmann_json::nlohmann_json
    )

    add_test(NAME TezTests COMMAND TezTests)

    message(STATUS "GTest found - tests enabled")
else()
    message(STATUS "GTest not found - tests disabled. Install with: brew install googletest (macOS) or apt-get install libgtest-dev (Ubuntu)")
endif()

