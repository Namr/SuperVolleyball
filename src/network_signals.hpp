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
  uint64_t command = RR_NO_REQUEST;
  int desired_room = -1;

  template<class Archive>
  void serialize(Archive & archive) {
    archive(command, desired_room);
  }
};

struct InputMessage {
  double delta_time = 0;
  bool up = false;
  bool down = false;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(delta_time, up, down);
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

#define RS_WAITING  0
#define RS_READY  1
#define RS_PLAYING  2

struct RoomState {
  uint16_t state = RS_WAITING;
  int current_room = -1;
  int num_connected = 0;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(state, current_room, num_connected);
  }
};

struct ServerNetworkMessage {
  std::vector<int> available_rooms;
  // which player you are in a room, -1 if not in room
  int player_number = -1;
  RoomState room_state;
  GameState game_state;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(available_rooms, player_number, room_state, game_state);
  }
};


void updatePlayerState(GameState& state, const InputMessage& input, uint8_t player);

void updateGameState(GameState& state, double delta_time);

void resetGameState(GameState& state);
