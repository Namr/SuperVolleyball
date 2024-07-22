#pragma once
#include <stdint.h>
#include <vector>
#include <cereal/types/vector.hpp>
#include <cereal/archives/binary.hpp>
#include "game_state.hpp"

#define RR_NO_REQUEST 0
#define RR_LIST_ROOMS 1
#define RR_JOIN_ROOM 2
#define RR_MAKE_ROOM 3

struct RoomRequest {
  uint16_t command = RR_NO_REQUEST;
  uint16_t desired_room = -1;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(command, desired_room);
  }
};

struct InputMessage {
  uint32_t tick = 0;
  bool up = false;
  bool down = false;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(tick, up, down);
  }
};

struct ClientNetworkMessage {
  RoomRequest room_request;
  std::vector<InputMessage> inputs;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(room_request, inputs);
  }
};

struct ServerNetworkMessage {
  std::vector<uint16_t> rooms;
  uint16_t current_room = -1;
  uint16_t num_players = 0;
  GameState game_state;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(rooms, current_room, num_players, game_state);
  }
};
