cmake_minimum_required(VERSION 3.16)
project(Tez)

set(CMAKE_CXX_STANDARD 17)

find_package(Boost REQUIRED)
find_package(nlohmann_json REQUIRED)
include_directories(${Boost_INCLUDE_DIRS})  # For header-only parts like Asio
include_directories(include)
add_executable(Tez src/main.cpp src/router.cpp src/middleware.cpp src/file_server.cpp)
target_link_libraries(Tez ${Boost_LIBRARIES}) 
target_link_libraries(Tez nlohmann_json::nlohmann_json)

