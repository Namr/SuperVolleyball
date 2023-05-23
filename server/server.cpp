#include "rtc/rtc.hpp"
#include <chrono>
#include <iostream>
#include <thread>
#include <variant>

#include "capnp/message.h"
#include "core/inputs.hpp"

int main() {
  rtc::WebSocketServer::Configuration server_config;
  server_config.enableTls = false;
  server_config.port = 8080;
  server_config.bindAddress = "127.0.0.1";
  rtc::WebSocketServer ws_server(server_config);

  std::vector<std::shared_ptr<rtc::WebSocket>> client_connections;

  ws_server.onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
    std::cout << "New Websocket client" << std::endl;

    ws->onOpen([]() { std::cout << "on open" << std::endl; });

    ws->onMessage([](std::variant<rtc::binary, rtc::string> message) {
      // TODO: check if message is actually binary
      svb_inputs::PlayerInputState input_state;
      rtc::binary binary_message = std::get<rtc::binary>(message);
      kj::ArrayPtr<capnp::word> message_data(
          reinterpret_cast<capnp::word *>(binary_message.data()),
          binary_message.size() * sizeof(std::byte));

      input_state.deserialize(message_data);
      if(input_state.hasKey(svb_inputs::PLAYER_UP)) {
          std::cout << "Player Up, ";
      }
      if(input_state.hasKey(svb_inputs::PLAYER_DOWN)) {
          std::cout << "Player Down, ";
      }
      if(input_state.hasKey(svb_inputs::PLAYER_LEFT)) {
          std::cout << "Player Left, ";
      }
      if(input_state.hasKey(svb_inputs::PLAYER_RIGHT)) {
          std::cout << "Player Right, ";
      }
      if(input_state.hasKey(svb_inputs::TARGET_UP)) {
          std::cout << "Target Up, ";
      }
      if(input_state.hasKey(svb_inputs::TARGET_DOWN)) {
          std::cout << "Target Down, ";
      }
      if(input_state.hasKey(svb_inputs::TARGET_LEFT)) {
          std::cout << "Target Left, ";
      }
      if(input_state.hasKey(svb_inputs::TARGET_RIGHT)) {
          std::cout << "Target Right, ";
      }
      if(input_state.hasKey(svb_inputs::PLAYER_JUMP)) {
          std::cout << "Player Jump, ";
      }
      std::cout << std::endl;
    });

    rtc::binary b;
    client_connections.push_back(ws);
  });

  while (true) {
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    svb_inputs::PlayerInputState input_state;
    input_state.addKey(svb_inputs::TARGET_DOWN);
    kj::Array<capnp::word> to_send = input_state.serialize();
    client_connections[0]->send(reinterpret_cast<std::byte *>(to_send.begin()),
                                to_send.size() * sizeof(capnp::word));
  }

  return 0;
}
