#pragma once
#include <cereal/archives/binary.hpp>
#include <stdint.h>

constexpr float arena_width = 800.0;
constexpr float arena_height = 450.0;
constexpr float paddle_width = 20.0;
constexpr float paddle_height = 60.0;
constexpr float paddle_speed = 150.0;
constexpr float ball_radius = 15.0;
constexpr float ball_speed = 200.0;

constexpr float EPSILON = 0.001;

struct Vec2 {
  float x = 0.0;
  float y = 0.0;


  template<class Archive>
  void serialize(Archive & archive) {
    archive(x, y);
  }

  bool operator ==(const Vec2 &c) {
    float e_x = std::abs(x - c.x);
    float e_y = std::abs(y - c.y);
    return e_y <= EPSILON && e_x <= EPSILON;
  }

  Vec2 operator +(const Vec2 &c) {
    Vec2 ret;
    ret.x = c.x + x;
    ret.y = c.y + y;
    return ret;
  }

  Vec2 operator -(const Vec2 &c) {
    Vec2 ret;
    ret.x = c.x - x;
    ret.y = c.y - y;
    return ret;
  }
};

struct PhysicsState {
  Vec2 pos;
  Vec2 vel;

  template<class Archive>
  void serialize(Archive & archive) {
    archive(pos, vel);
  }

  bool operator ==(const PhysicsState &c) {
    return pos == c.pos && vel == c.vel;
  }
};

struct GameState {
  PhysicsState p1_paddle;
  PhysicsState p2_paddle;
  PhysicsState ball;
  uint16_t p1_score = 0;
  uint16_t p2_score = 0;

  template<class Archive>
  void serialize(Archive & archive) {
    archive(p1_paddle, p2_paddle, ball, p1_score, p2_score);
  }

  bool operator ==(const GameState& c) {
    return p1_paddle == c.p1_paddle && p2_paddle == c.p2_paddle && ball == c.ball && p1_score == c.p1_score && p2_score == c.p2_score;
  }

  bool operator !=(const GameState& c) {
    return !(*this == c);
  }
};
