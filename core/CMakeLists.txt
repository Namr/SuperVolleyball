cmake_minimum_required(VERSION 3.13)

project(supervolleyball-core)
set(CMAKE_CXX_STANDARD 17)

# capn proto 
add_definitions(${CAPNP_DEFINITIONS})
set(CAPNPC_SRC_PREFIX "${CMAKE_CURRENT_SOURCE_DIR}" CACHE STRING "" FORCE)
set(CAPNPC_OUTPUT_DIR "." CACHE STRING "" FORCE)
file(MAKE_DIRECTORY "${CAPNPC_OUTPUT_DIR}")

if (EMSCRIPTEN)
  list(APPEND ${CAPNP_SOURCES} "${CMAKE_CURRENT_BINARY_DIR}/core/proto/input.capnp.c++")
  list(APPEND ${CAPNP_HEADERS} "${CMAKE_CURRENT_BINARY_DIR}/core/proto/input.capnp.h")
else()
capnp_generate_cpp(CAPNP_SOURCES CAPNP_HEADERS proto/input.capnp)
endif()

add_library(${PROJECT_NAME} inputs.cpp ${CAPNP_SOURCES})
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR} ${CAPNP_INCLUDE_DIRS} ${CMAKE_BINARY_DIR})
target_link_libraries(${PROJECT_NAME} CapnProto::capnp)
install(TARGETS ${PROJECT_NAME} DESTINATION ${CMAKE_SOURCE_DIR}/bin)
