#pragma once
#include "game_state.hpp"
#include <cereal/archives/binary.hpp>
#include <cereal/types/vector.hpp>
#include <stdint.h>
#include <vector>

constexpr uint16_t MSG_LOBBY_STATE = 0;
constexpr uint16_t MSG_ROOM_REQUEST = 1;
constexpr uint16_t MSG_CLIENT_INPUT = 2;
constexpr uint16_t MSG_ROOM_STATE = 3;
constexpr uint16_t MSG_GAME_STATE = 4;

struct MessageTag {
  uint16_t type;

  template <class Archive> void serialize(Archive &archive) { archive(type); }
};

constexpr uint16_t RR_NO_REQUEST = 0;
constexpr uint16_t RR_LIST_ROOMS = 1;
constexpr uint16_t RR_JOIN_ROOM = 2;
constexpr uint16_t RR_MAKE_ROOM = 3;

struct RoomRequest {
  uint16_t command = RR_NO_REQUEST;
  int desired_room = -1;

  template <class Archive> void serialize(Archive &archive) {
    archive(command, desired_room);
  }
};

struct LobbyState {
  std::vector<int> available_rooms;

  template <class Archive> void serialize(Archive &archive) {
    archive(available_rooms);
  }
};

constexpr uint16_t RS_WAITING = 0;
constexpr uint16_t RS_PLAYING = 1;

struct RoomState {
  uint16_t state = RS_WAITING;
  int current_room = -1;
  int num_connected = 0;
  int player_index = -1;

  template <class Archive> void serialize(Archive &archive) {
    archive(state, current_room, num_connected, player_index);
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

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player);

void updateGameState(GameState &state, double delta_time);

void resetGameState(GameState &state);
