cmake_minimum_required(VERSION 3.13)

project(supervolleyball-server)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} server.cpp game_objects.cpp)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CAPNP_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} supervolleyball-core LibDataChannel::LibDataChannel)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
