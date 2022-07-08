# cmake build file for C++ Seeder Server and client.

cmake_minimum_required(VERSION 3.5.1)

project(SeederServerClient C CXX)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

find_package(Threads REQUIRED)

include_directories("${CMAKE_CURRENT_BINARY_DIR}")

# Strip symbols
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s")

# Server source code
set(server_srcs  server/server.cpp)

# Client source code
set(client_srcs  client/client.cpp)

# Server binary
add_executable(server ${server_srcs})

# Client binary
add_executable(client ${client_srcs})