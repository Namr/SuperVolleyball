# C++ config
cmake_minimum_required(VERSION 3.10)

project(supervolleyball)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# dependencies
add_subdirectory(deps/GameNetworkingSockets)
add_subdirectory(deps/raylib)

# client
add_executable(svb_client src/client.cpp)
target_include_directories(svb_client PRIVATE ${CMAKE_SOURCE_DIR} deps/cereal/include ${CMAKE_BINARY_DIR})
target_link_libraries(svb_client raylib GameNetworkingSockets)

# server
add_executable(svb_server src/server.cpp)
target_include_directories(svb_server PRIVATE ${CMAKE_SOURCE_DIR} deps/cereal/include ${CMAKE_BINARY_DIR})
target_link_libraries(svb_server GameNetworkingSockets)
