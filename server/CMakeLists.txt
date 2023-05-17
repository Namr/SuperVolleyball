cmake_minimum_required(VERSION 3.13)

project(supervolleyball-server)
set(CMAKE_CXX_STANDARD 17)

add_executable(${PROJECT_NAME} server.cpp)
target_link_libraries(${PROJECT_NAME} LibDataChannel::LibDataChannel)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
