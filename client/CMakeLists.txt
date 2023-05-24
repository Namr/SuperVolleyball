cmake_minimum_required(VERSION 3.13)

project(supervolleyball-client)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} client.cpp input_reader.cpp entity.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CAPNP_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} supervolleyball-core raylib datachannel-wasm)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
