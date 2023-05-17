cmake_minimum_required(VERSION 3.13)

project(supervolleyball-client)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} client.cpp)
target_link_libraries(${PROJECT_NAME} raylib datachannel-wasm)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
