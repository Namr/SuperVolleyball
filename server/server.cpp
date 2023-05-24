#include "rtc/rtc.hpp"
#include <chrono>
#include <iostream>
#include <queue>
#include <thread>
#include <variant>

#include "capnp/message.h"
#include "core/inputs.hpp"
#include "server/game_objects.hpp"

constexpr int ticks_per_second = 60;
constexpr std::chrono::duration<double> time_per_tick(1.0 / ticks_per_second);

struct ClientSession {
  std::shared_ptr<rtc::WebSocket> socket;
  std::shared_ptr<svb::Player> player;
};

int main() {
  // setup connections to clients in an async manner
  rtc::WebSocketServer::Configuration server_config;
  server_config.enableTls = false;
  server_config.port = 8080;
  server_config.bindAddress = "127.0.0.1";
  rtc::WebSocketServer ws_server(server_config);

  std::vector<std::shared_ptr<ClientSession>> client_connections;
  svb::World world;

  ws_server.onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
    std::cout << "new player connected" << std::endl;

    // Init our session with this client
    std::shared_ptr<svb::Player> p(new svb::Player());
    std::shared_ptr<ClientSession> session(new ClientSession());
    world.players.push_back(p);
    session->socket = ws;
    session->player = p;
    client_connections.push_back(session);

    ws->onOpen([]() {
      // note (amoussa): this should really be checked before we send anything
      // to client but nothing has gone wrong so far....
      std::cout << "player websocket open & ready to use" << std::endl;
    });

    // note (amoussa): my intention is to *copy* the shared_ptr such that the
    // lambda class is now an owner of this session. I believe the lambda is
    // deleted and freed when the websocket is freed, thus also releasing
    // ownership of the shared pointer (and it should be the only owner) but:
    // TODO: this should be explicitly tested
    ws->onMessage([session](std::variant<rtc::binary, rtc::string> message) {
      if (std::holds_alternative<rtc::binary>(message)) {

        // unpack the message
        svb::input::PlayerInputState input_state;
        rtc::binary binary_message = std::get<rtc::binary>(message);
        kj::ArrayPtr<capnp::word> message_data(
            reinterpret_cast<capnp::word *>(binary_message.data()),
            binary_message.size() * sizeof(std::byte));

        input_state.deserialize(message_data);

        // update the player accordingly
        session->player->update(input_state);
      } else {
        std::cout << "The server got a string message, this is unexpected."
                  << std::endl;
      }
    });
  });

  // main loop
  while (true) {
    auto target_next_tick_time =
        std::chrono::system_clock::now() + time_per_tick;

    // game logic
    // note (amoussa): the intention here is that this loop copies the
    // shared_ptr for only as long as it is running (to ensure it is not freed)
    // and then immediatly releases it.
    for (std::shared_ptr<svb::Player> p : world.players) {
      p->tick();
    }

    // send world to clients
    kj::Array<capnp::word> to_send = world.serialize();
    for (std::shared_ptr<ClientSession> session : client_connections) {
      session->socket->send(reinterpret_cast<std::byte *>(to_send.begin()),
                           to_send.size() * sizeof(capnp::word));
    }

    // wait for next tick
    auto time_to_wait =
        target_next_tick_time - std::chrono::system_clock::now();
    if (time_to_wait.count() > 0) {
      std::this_thread::sleep_for(time_to_wait);
    } else {
      std::cout
          << "We spent too long on a tick!! Consider decreasing the tick rate."
          << std::endl;
    }
  }

  return 0;
}
