#include "raylib.h"
#include "rtc/rtc.hpp"
#include <iostream>

#include "client/entity.hpp"
#include "client/input_reader.hpp"

int main() {

  svb::EntityList world;

  rtc::WebSocket ws;

  ws.onOpen([&]() { std::cout << "WebSocket open" << std::endl; });

  ws.onError([](std::string error) {
    std::cout << "WebSocket error: " << error << std::endl;
  });

  ws.onClosed([]() { std::cout << "WebSocket closed" << std::endl; });

  ws.onMessage([&](std::variant<rtc::binary, rtc::string> message) {
    if (std::holds_alternative<rtc::binary>(message)) {
      // unpack the message
      rtc::binary binary_message = std::get<rtc::binary>(message);
      kj::ArrayPtr<capnp::word> message_data(
          reinterpret_cast<capnp::word *>(binary_message.data()),
          binary_message.size() * sizeof(std::byte));

      world.deserialize(message_data);
    } else {
      std::cout << "The client got a string message, this is unexpected."
                << std::endl;
    }
  });

  ws.open("ws://127.0.0.1:8080");

  // Initialization
  const int canvas_width = 1024;
  const int canvas_height = 576;

  InitWindow(canvas_width, canvas_height, "Super Volleyball");

  SetTargetFPS(60);

  std::chrono::duration<double> delta_time(0.0);
  auto start = std::chrono::system_clock::now();

  // Main game loop
  while (!WindowShouldClose()) {
    delta_time = std::chrono::system_clock::now() - start;
    start = std::chrono::system_clock::now();
    // send inputs to server
    kj::Array<capnp::word> to_send =
        svb::input::getCurrentInputState().serialize();
    ws.send(reinterpret_cast<std::byte *>(to_send.begin()),
            to_send.size() * sizeof(capnp::word));

    // Draw
    BeginDrawing();

    ClearBackground(RAYWHITE);

    for (svb::Entity &e : world.entities) {
      e.render();
      e.tick(delta_time.count());
    }

    EndDrawing();
  }

  CloseWindow();
  return 0;
}
