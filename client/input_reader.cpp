#include "client/input_reader.hpp"
#include "raylib.h"

namespace svb {
namespace input {

// TODO: allow players to rebind their keys
PlayerInputState getCurrentInputState() {
  PlayerInputState state;

  if (IsKeyDown(KEY_W)) {
    state.addKey(PLAYER_UP);
  }

  if (IsKeyDown(KEY_S)) {
    state.addKey(PLAYER_DOWN);
  }

  if (IsKeyDown(KEY_A)) {
    state.addKey(PLAYER_LEFT);
  }

  if (IsKeyDown(KEY_D)) {
    state.addKey(PLAYER_RIGHT);
  }

  if (IsKeyDown(KEY_I)) {
    state.addKey(TARGET_UP);
  }

  if (IsKeyDown(KEY_K)) {
    state.addKey(TARGET_DOWN);
  }

  if (IsKeyDown(KEY_J)) {
    state.addKey(TARGET_LEFT);
  }

  if (IsKeyDown(KEY_L)) {
    state.addKey(TARGET_RIGHT);
  }

  if (IsKeyDown(KEY_SPACE)) {
    state.addKey(PLAYER_JUMP);
  }

  return state;
}

} // namespace input
} // namespace svb
