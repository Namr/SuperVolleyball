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

  int num_connected = 0;
  std::array<std::optional<std::shared_ptr<ClientSession>>, svb::max_players> client_connections;
  svb::World world;

  ws_server.onClient([&](std::shared_ptr<rtc::WebSocket> ws) {
    std::cout << "new player connected" << std::endl;

    if (num_connected < svb::max_players) {

      num_connected++;
      int position = -1;
      for (int i = 0; i < svb::max_players; i++) {
        if (!client_connections.at(i)) {
          position = i;
          break;
        }
      }

      // error checking an absurd case
      if (position < 0) {
        std::cout << "ERROR: num_connected != number of connections, this "
                     "should NEVER happen"
                  << std::endl;
        return;
      }

      // Init our session with this client
      std::shared_ptr<svb::Player> p(new svb::Player(position));
      std::shared_ptr<ClientSession> session(new ClientSession());
      session->socket = ws;
      session->player = p;
      world.players.at(position) = p;
      client_connections.at(position) = session;

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

      ws->onClosed([&world, &num_connected, &client_connections, position]() {
        std::cout << "Closing Websocket connection" << std::endl;
        num_connected--;
        client_connections.at(position) = std::nullopt;
        world.players.at(position) = std::nullopt;
      });
      ws->onError([&ws, &client_connections, position](std::string error_msg) {
        std::cout << "Websocket ERROR: " << error_msg << std::endl;
        ws->close();
      });
    } else {
      std::cout << "Server full, refusing connection to new player"
                << std::endl;
      ws->close();
    }
  });

  // main loop
  std::chrono::duration<double> delta_time(0.0);
  while (true) {
    auto start = std::chrono::system_clock::now();
    auto target_next_tick_time = start + time_per_tick;

    // game logic
    // TODO: instead of just pausing ticks, not having enough players should
    // display some sort of waiting message for the client
    if (num_connected == svb::max_players) {
      // note (amoussa): the intention here is that this loop copies the
      // shared_ptr for only as long as it is running (to ensure it is not
      // freed) and then releases it at the end of the loop.
      for (std::optional<std::shared_ptr<svb::Player>> p : world.players) {
        if (p) {
          // collision checking
          if ((p.value()->getPosition() - world.ball.getPosition()).norm() <
              p.value()->getRadius() + world.ball.getRadius()) {
            p.value()->onBallCollision(world.ball);
          }

          p.value()->tick(delta_time.count());
        }
      }
      world.ball.tick(delta_time.count());
    }

    // send world to clients
    // note (amoussa): I'm unsure if send happens async or is blocking. If it's
    // blocking maybe this should be done in a different thread?
    kj::Array<capnp::word> to_send = world.serialize();
    for (std::optional<std::shared_ptr<ClientSession>> session :
         client_connections) {
      if (session) {
        session.value()->socket->send(
            reinterpret_cast<std::byte *>(to_send.begin()),
            to_send.size() * sizeof(capnp::word));
      }
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

    delta_time = std::chrono::system_clock::now() - start;
  }

  return 0;
}
