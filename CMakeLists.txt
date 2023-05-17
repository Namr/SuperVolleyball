cmake_minimum_required(VERSION 3.13)

project(supervolleyball)
set(CMAKE_CXX_STANDARD 17)

if (EMSCRIPTEN)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_GLFW=3 -s ASSERTIONS=1 -s WASM=1 -s ASYNCIFY")
  set(CMAKE_EXECUTABLE_SUFFIX ".html")
  add_subdirectory(deps/datachannel-wasm)
  add_subdirectory(deps/raylib)
  add_subdirectory(client)
else()
  add_subdirectory(deps/libdatachannel)
  add_subdirectory(server)
endif ()

