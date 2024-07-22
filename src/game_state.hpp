#pragma once
#include <cereal/archives/binary.hpp>
#include <stdint.h>

struct Vec2 {
  float x = 0.0;
  float y = 0.0;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(x, y);
  }
};

struct PhysicsState {
  Vec2 pos;
  Vec2 vel;

  template<class Archive>
  void serialize(Archive & archive) {
    archive(pos, vel);
  }
};

struct GameState {
  uint32_t tick = 0;
  PhysicsState p1_paddle;
  PhysicsState p2_paddle;
  PhysicsState ball;
  uint16_t p1_score = 0;
  uint16_t p2_score = 0;

  template<class Archive>
  void serialize(Archive & archive) {
    archive(tick, p1_paddle, p2_paddle, ball, p1_score, p2_score);
  }
};
