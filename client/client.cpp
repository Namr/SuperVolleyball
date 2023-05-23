#include "raylib.h"
#include "rtc/rtc.hpp"
#include <iostream>

#include "client/input_reader.hpp"

int main() {
  rtc::WebSocket ws;

  ws.onOpen([&]() { std::cout << "WebSocket open" << std::endl; });

  ws.onError([](std::string error) {
    std::cout << "WebSocket error: " << error << std::endl;
  });

  ws.onClosed([]() { std::cout << "WebSocket closed" << std::endl; });

  ws.onMessage([&](std::variant<rtc::binary, rtc::string> message) {
      // TODO: do something
  });

  ws.open("ws://127.0.0.1:8080");

  // Initialization
  const int screenWidth = 800;
  const int screenHeight = 450;

  InitWindow(screenWidth, screenHeight, "raylib [core] example - basic window");

  SetTargetFPS(20); // Set our game to run at 60 frames-per-second

  // Main game loop
  while (!WindowShouldClose()) // Detect window close button or ESC key
  {
    // send inputs to server
    kj::Array<capnp::word> to_send = svb_inputs::getCurrentInputState().serialize();
    ws.send(reinterpret_cast<std::byte *>(to_send.begin()),
                                to_send.size() * sizeof(capnp::word));

    // Draw
    BeginDrawing();

    ClearBackground(RAYWHITE);

    DrawText("Congrats! You created your first window!", 190, 200, 20,
             LIGHTGRAY);
    
    EndDrawing();
  }

  CloseWindow(); // Close window and OpenGL context
  return 0;
}
