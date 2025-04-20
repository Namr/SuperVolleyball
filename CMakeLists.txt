# C++ config
cmake_minimum_required(VERSION 3.10)

project(supervolleyball)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# dependencies
add_subdirectory(deps/GameNetworkingSockets)
add_subdirectory(deps/raylib)

# client
add_executable(svb_client src/client.cpp src/game_state.cpp)
target_include_directories(svb_client PRIVATE ${CMAKE_SOURCE_DIR} deps/cereal/include ${CMAKE_BINARY_DIR})
target_link_libraries(svb_client raylib GameNetworkingSockets::GameNetworkingSockets_s)

# server
add_executable(svb_server src/server.cpp src/game_state.cpp)
target_include_directories(svb_server PRIVATE ${CMAKE_SOURCE_DIR} deps/cereal/include ${CMAKE_BINARY_DIR})
target_link_libraries(svb_server GameNetworkingSockets::GameNetworkingSockets_s)

set_target_properties(svb_client PROPERTIES LINK_FLAGS "-Wl,-rpath,./")
set_target_properties(svb_server PROPERTIES LINK_FLAGS "-Wl,-rpath,./")
