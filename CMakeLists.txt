cmake_minimum_required(VERSION 3.20)
project(Tez VERSION 1.1.0 LANGUAGES CXX)

if(NOT UNIX)
    message(FATAL_ERROR "Tez currently supports Linux and macOS (POSIX file access is required).")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(CTest)
include(GNUInstallDirs)
option(TEZ_WARNINGS_AS_ERRORS "Treat project compiler warnings as errors" OFF)
option(TEZ_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(TEZ_ENABLE_TSAN "Enable ThreadSanitizer (separate build required)" OFF)
set(TEZ_VERSION "${PROJECT_VERSION}-dev" CACHE STRING "Version reported by the server")

if(TEZ_ENABLE_SANITIZERS AND TEZ_ENABLE_TSAN)
    message(FATAL_ERROR "AddressSanitizer and ThreadSanitizer require separate builds.")
endif()

find_package(Boost 1.74 REQUIRED CONFIG)
find_package(nlohmann_json 3.7 REQUIRED CONFIG)
find_package(Threads REQUIRED)

add_library(tez_options INTERFACE)
target_compile_options(tez_options INTERFACE -Wall -Wextra -Wpedantic)
if(TEZ_WARNINGS_AS_ERRORS)
    target_compile_options(tez_options INTERFACE -Werror)
endif()
if(TEZ_ENABLE_SANITIZERS OR TEZ_ENABLE_TSAN)
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        message(FATAL_ERROR "Sanitizer builds require GCC or Clang.")
    endif()
    if(TEZ_ENABLE_TSAN)
        set(TEZ_SANITIZER thread)
    else()
        set(TEZ_SANITIZER address,undefined)
    endif()
    target_compile_options(tez_options INTERFACE -fsanitize=${TEZ_SANITIZER} -fno-omit-frame-pointer)
    target_link_options(tez_options INTERFACE -fsanitize=${TEZ_SANITIZER})
endif()

add_library(TezLib STATIC
    src/router.cpp
    src/middleware.cpp
    src/file_server.cpp
    src/request.cpp
)
target_include_directories(TezLib PUBLIC include)
target_link_libraries(TezLib PUBLIC Boost::headers nlohmann_json::nlohmann_json Threads::Threads tez_options)

add_executable(Tez src/main.cpp)
target_link_libraries(Tez PRIVATE TezLib)
target_compile_definitions(Tez PRIVATE TEZ_VERSION="${TEZ_VERSION}")

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    find_package(Python3 3.8 REQUIRED COMPONENTS Interpreter)
    add_executable(TezTests
        tests/test_router.cpp
        tests/test_middleware.cpp
        tests/test_file_server.cpp
        tests/test_response.cpp
        tests/test_request.cpp
    )
    target_link_libraries(TezTests PRIVATE TezLib GTest::gtest_main)
    add_test(NAME unit COMMAND TezTests)
    add_test(NAME http_integration
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_http_integration.py --server $<TARGET_FILE:Tez>)
    set_tests_properties(unit PROPERTIES TIMEOUT 60)
    set_tests_properties(http_integration PROPERTIES TIMEOUT 90)
endif()

install(TARGETS Tez RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES config.json DESTINATION ${CMAKE_INSTALL_DATADIR}/tez)
install(DIRECTORY static/ DESTINATION ${CMAKE_INSTALL_DATADIR}/tez/static)
install(FILES LICENSE DESTINATION ${CMAKE_INSTALL_DATADIR}/licenses/tez)
