#pragma once
#include "network_signals.hpp"
#include <cereal/archives/binary.hpp>
#include <stdint.h>

constexpr size_t MAX_ROOMS = 16;
constexpr double TICK_RATE = 64.0;
constexpr double DESIRED_TICK_LENGTH = 1.0 / TICK_RATE;

constexpr float arena_width = 800.0;
constexpr float arena_height = 450.0;
constexpr float paddle_width = 25.0;
constexpr float paddle_height = 25.0;
constexpr float starting_dist_from_screen = 20.0;
constexpr float paddle_speed = 160.0;
constexpr float target_speed = 160.0;
constexpr float target_radius = 10.0;
constexpr float ball_radius = 15.0;
constexpr float init_ball_speed = 100.0;
constexpr float max_ball_speed = 300.0;
constexpr float ball_up_speed = 50.0;
constexpr float ball_speed_inc = 50.0;
constexpr float max_bounce_angle = 35.0;
constexpr float center_line_width = 10.0;

constexpr float service_hittable_time = 0.5;
constexpr float service_max_time = 0.8;

constexpr double EPSILON = 0.8;
inline bool fcmp(float a, float b) { return std::abs(a - b) < EPSILON; }

struct Vec3 {
  float x = 0.0;
  float y = 0.0;
  float z = 0.0;

  template <class Archive> void serialize(Archive &archive) {
    archive(x, y, z);
  }

  bool operator==(const Vec3 &c) {
    return fcmp(x, c.x) && fcmp(y, c.y) && fcmp(z, c.z);
  }

  Vec3 operator+(const Vec3 &c) {
    Vec3 ret;
    ret.x = c.x + x;
    ret.y = c.y + y;
    ret.z = c.z + z;
    return ret;
  }

  Vec3 operator-(const Vec3 &c) {
    Vec3 ret;
    ret.x = c.x - x;
    ret.y = c.y - y;
    ret.z = c.z + z;
    return ret;
  }
};

struct PhysicsState {
  Vec3 pos;
  Vec3 vel;

  template <class Archive> void serialize(Archive &archive) {
    archive(pos, vel);
  }

  bool operator==(const PhysicsState &c) {
    return pos == c.pos && vel == c.vel;
  }
};

constexpr uint32_t BALL_STATE_READY_TO_SERVE = 0;
constexpr uint32_t BALL_STATE_IN_SERVICE = 1;
constexpr uint32_t BALL_STATE_TRAVELLING = 2;
constexpr uint32_t BALL_STATE_FAILED_SERVICE = 3;

struct GameState {
  PhysicsState p1;
  PhysicsState p2;
  PhysicsState p3;
  PhysicsState p4;
  PhysicsState ball;
  PhysicsState target;
  uint16_t p1_score = 0;
  uint16_t p2_score = 0;
  float ball_speed = 0.0;
  uint32_t tick = 0;
  uint32_t ball_state = BALL_STATE_READY_TO_SERVE;
  uint32_t ball_owner = 1; // who is serving the ball right now 1-4; 0 -> no-one
  float timer = 0.0;

  template <class Archive> void serialize(Archive &archive) {
    archive(p1, p2, p3, p4, ball, target, p1_score, p2_score, ball_speed, tick,
            ball_state, ball_owner, timer);
  }

  bool operator==(const GameState &c) {
    return p1 == c.p1 && p2 == c.p2 && p3 == c.p3 && p4 == c.p4 &&
           ball == c.ball && target == c.target && p1_score == c.p1_score &&
           p2_score == c.p2_score && fcmp(ball_speed, c.ball_speed) &&
           tick == c.tick && ball_state == c.ball_state &&
           ball_owner == c.ball_owner && fcmp(timer, c.timer);
  }

  bool operator!=(const GameState &c) { return !(*this == c); }
};

Vec3 interpolate(Vec3 &previous, Vec3 &next, double a);
PhysicsState interpolate(PhysicsState &previous, PhysicsState &next, double a);
GameState interpolate(GameState &previous, GameState &next, double a);

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player);

void updateGameState(GameState &state, double delta_time);

void resetGameState(GameState &state);
