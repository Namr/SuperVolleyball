#pragma once
#include "game_state.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <stdint.h>
#include <vector>

constexpr int RR_NO_REQUEST = 0;
constexpr int RR_LIST_ROOMS = 1;
constexpr int RR_JOIN_ROOM = 2;
constexpr int RR_MAKE_ROOM = 3;

struct RoomRequest {
  uint64_t command = RR_NO_REQUEST;
  int desired_room = -1;

  template <class Archive> void serialize(Archive &archive) {
    archive(command, desired_room);
  }
};

struct InputMessage {
  uint32_t tick = 0;
  bool up = false;
  bool down = false;

  template <class Archive> void serialize(Archive &archive) {
    archive(tick, up, down);
  }
};

struct ClientNetworkMessage {
  RoomRequest room_request;
  std::vector<InputMessage> inputs;

  template <class Archive> void serialize(Archive &archive) {
    archive(room_request, inputs);
  }
};

constexpr int RS_WAITING = 0;
constexpr int RS_PLAYING = 1;

struct RoomState {
  uint16_t state = RS_WAITING;
  int current_room = -1;
  int num_connected = 0;

  template <class Archive> void serialize(Archive &archive) {
    archive(state, current_room, num_connected);
  }
};

struct ServerNetworkMessage {
  std::vector<int> available_rooms; // TODO: change to bit field
  // which player you are in a room, -1 if not in room
  int player_number = -1;
  RoomState room_state;
  GameState game_state;

  template <class Archive> void serialize(Archive &archive) {
    archive(available_rooms, player_number, room_state, game_state);
  }
};

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player);

void updateGameState(GameState &state, double delta_time);

void resetGameState(GameState &state);
