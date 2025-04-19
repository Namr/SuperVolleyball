#pragma once
#include "network_signals.hpp"
#include <cereal/archives/binary.hpp>
#include <math.h>
#include <stdint.h>

constexpr size_t MAX_ROOMS = 16;
constexpr double TICK_RATE = 64.0;
constexpr double DESIRED_TICK_LENGTH = 1.0 / TICK_RATE;

constexpr float arena_width = 800.0;
constexpr float arena_height = 450.0;
constexpr float paddle_width = 25.0;
constexpr float paddle_height = 25.0;
constexpr float starting_dist_from_screen = 20.0;
constexpr float paddle_speed = 195.0;
constexpr float target_speed = 225.0;
constexpr float target_radius = 10.0;
constexpr float ball_radius = 15.0;
constexpr float hit_leeway = -0.1;
constexpr float ball_serving_speed = 400.0;
constexpr float ball_shooting_speed = 250.0;
constexpr float ball_spiking_speed = 500.0;
constexpr float ball_blocked_speed = 400.0;
constexpr float ball_up_speed = 20.0;
constexpr float max_bounce_angle = 35.0;
constexpr float center_line_width = 10.0;
constexpr float ball_max_passing_height = 30.0;
constexpr float jump_speed = 130.0;
constexpr float jump_height = 25.0;
constexpr float passing_max_dist = 100.0;
constexpr float passing_min_dist = -100.0;
constexpr float hitting_max_z_dist = 6.0;
constexpr float spiking_min_player_z = 6.0;
constexpr float bumping_xy_penalty = 70.0;
constexpr float blocking_max_dist_from_center =
    paddle_width + center_line_width + 5.0;
constexpr float blocking_min_height = jump_height - 5.0;

// all in seconds
constexpr float service_hittable_time = 0.5;
constexpr float service_max_time = 2.5;
constexpr float ball_passing_time = 2.5;
constexpr float game_over_grace_period = 0.5;

constexpr double EPSILON = 0.8;

constexpr float Z_TO_SIZE_RATIO = 0.3;

inline bool fcmp(float a, float b) { return std::abs(a - b) < EPSILON; }

struct Vec3 {
  float x = 0.0;
  float y = 0.0;
  float z = 0.0;

  template <class Archive> void serialize(Archive &archive) {
    archive(x, y, z);
  }

  float magnitude2D() { return std::sqrt((x * x) + (y * y)); }

  bool operator==(const Vec3 &c) {
    return fcmp(x, c.x) && fcmp(y, c.y) && fcmp(z, c.z);
  }

  Vec3 operator+(const Vec3 &c) const {
    Vec3 ret;
    ret.x = c.x + x;
    ret.y = c.y + y;
    ret.z = c.z + z;
    return ret;
  }

  Vec3 operator-(const Vec3 &c) const {
    Vec3 ret;
    ret.x = c.x - x;
    ret.y = c.y - y;
    ret.z = c.z + z;
    return ret;
  }

  Vec3 operator*(const float c) const {
    Vec3 ret = *this;
    ret.x *= c;
    ret.y *= c;
    ret.z *= c;
    return ret;
  }

  void operator*=(const float c) {
    x *= c;
    y *= c;
    z *= c;
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
constexpr uint32_t BALL_STATE_FIRST_PASS = 4;
constexpr uint32_t BALL_STATE_SECOND_PASS = 5;
constexpr uint32_t BALL_STATE_GAME_OVER = 6;

struct GameState {
  PhysicsState p1;
  PhysicsState p2;
  PhysicsState p3;
  PhysicsState p4;
  PhysicsState ball;
  PhysicsState target;
  PhysicsState landing_zone;
  uint16_t team1_score = 0;
  uint16_t team2_score = 0;
  uint16_t team1_points_to_give =
      0; // dumb but this way the score doesn't update right away
  uint16_t team2_points_to_give = 0;
  uint32_t tick = 0;
  uint32_t ball_state = BALL_STATE_READY_TO_SERVE;
  uint8_t last_server = 1;
  int16_t ball_owner = 1; // who is serving the ball right now 1-4; 0 -> no-one
  bool can_owner_move = false;
  float timer = 0.0;

  template <class Archive> void serialize(Archive &archive) {
    archive(p1, p2, p3, p4, ball, target, landing_zone, team1_score,
            team2_score, team1_points_to_give, team2_points_to_give, tick,
            ball_state, last_server, ball_owner, can_owner_move, timer);
  }

  bool operator==(const GameState &c) {
    return p1 == c.p1 && p2 == c.p2 && p3 == c.p3 && p4 == c.p4 &&
           ball == c.ball && target == c.target &&
           landing_zone == c.landing_zone && team1_score == c.team1_score &&
           team2_score == c.team2_score &&
           team1_points_to_give == c.team1_points_to_give &&
           team2_points_to_give == c.team2_points_to_give && tick == c.tick &&
           ball_state == c.ball_state && last_server == c.last_server &&
           ball_owner == c.ball_owner && can_owner_move == c.can_owner_move &&
           fcmp(timer, c.timer);
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
