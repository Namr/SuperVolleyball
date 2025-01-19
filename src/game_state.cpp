#include "game_state.hpp"
#include <algorithm>
#include <math.h>

Vec2 interpolate(Vec2 &previous, Vec2 &next, double a) {
  Vec2 ret;
  ret.x = next.x * a + previous.x * (1.0 - a);
  ret.y = next.y * a + previous.y * (1.0 - a);
  return ret;
}

PhysicsState interpolate(PhysicsState &previous, PhysicsState &next, double a) {
  PhysicsState ret;
  ret.vel = interpolate(previous.vel, next.vel, a);
  ret.pos = interpolate(previous.pos, next.pos, a);
  return ret;
}

GameState interpolate(GameState &previous, GameState &next, double a) {
  GameState ret;
  ret.p1 = interpolate(previous.p1, next.p1, a);
  ret.p2 = interpolate(previous.p2, next.p2, a);
  ret.p3 = interpolate(previous.p3, next.p3, a);
  ret.p4 = interpolate(previous.p4, next.p4, a);
  ret.ball = interpolate(previous.ball, next.ball, a);

  // things that should just snap to previous
  ret.ball_speed = previous.ball_speed;
  ret.p1_score = previous.p1_score;
  ret.p2_score = previous.p2_score;
  ret.tick = previous.tick;
  return ret;
}
void resetGameState(GameState &state) {
  state.p1.vel.x = 0;
  state.p1.vel.y = 0;
  state.p1.pos.x = 0;
  state.p1.pos.y = arena_height / 4.0;

  state.p2.vel.x = 0;
  state.p2.vel.y = 0;
  state.p2.pos.x = 0;
  state.p2.pos.y = 3.0f * (arena_height / 4.0);

  state.p3.vel.x = 0;
  state.p3.vel.y = 0;
  state.p3.pos.x = arena_width - paddle_width;
  state.p3.pos.y = arena_height / 4.0;

  state.p4.vel.x = 0;
  state.p4.vel.y = 0;
  state.p4.pos.x = arena_width - paddle_width;
  state.p4.pos.y = 3.0f * (arena_height / 4.0);

  state.ball_speed = init_ball_speed;
  state.ball.vel.x = -state.ball_speed;
  state.ball.vel.y = 0;
  state.ball.pos.x = arena_width / 2.0;
  state.ball.pos.y = arena_height / 2.0;

  state.p1_score = 0;
  state.p2_score = 0;
}

void resetRound(GameState &state) {
  state.p1.vel.x = 0;
  state.p1.vel.y = 0;
  state.p1.pos.x = 0;
  state.p1.pos.y = arena_height / 4.0;

  state.p2.vel.x = 0;
  state.p2.vel.y = 0;
  state.p2.pos.x = 0;
  state.p2.pos.y = 3.0f * (arena_height / 4.0);

  state.p3.vel.x = 0;
  state.p3.vel.y = 0;
  state.p3.pos.x = arena_width - paddle_width;
  state.p3.pos.y = arena_height / 4.0;

  state.p4.vel.x = 0;
  state.p4.vel.y = 0;
  state.p4.pos.x = arena_width - paddle_width;
  state.p4.pos.y = 3.0f * (arena_height / 4.0);

  state.ball.vel.x = -state.ball_speed;
  state.ball.vel.y = 0;
  state.ball.pos.x = arena_width / 2.0;
  state.ball.pos.y = arena_height / 2.0;
}

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player) {
  PhysicsState *paddle = nullptr;
  switch (player) {
  case 0:
    paddle = &state.p1;
    break;
  case 1:
    paddle = &state.p2;
    break;
  case 2:
    paddle = &state.p3;
    break;
  case 3:
  default:
    paddle = &state.p4;
    break;
  }

  if (input.up) {
    paddle->vel.y = -paddle_speed;
  }
  if (input.down) {
    paddle->vel.y = paddle_speed;
  }

  if ((input.up | input.down) == false) {
    paddle->vel.y = 0.0;
  }

  if (input.left) {
    paddle->vel.x = -paddle_speed;
  }
  if (input.right) {
    paddle->vel.x = paddle_speed;
  }

  if ((input.left | input.right) == false) {
    paddle->vel.x = 0.0;
  }

  // normalize velocity
  float magnitude = std::sqrt((paddle->vel.x * paddle->vel.x) +
                              (paddle->vel.y * paddle->vel.y));
  if (magnitude > 0.01) {
    paddle->vel.x = (paddle->vel.x / magnitude) * paddle_speed;
    paddle->vel.y = (paddle->vel.y / magnitude) * paddle_speed;
  }

  paddle->pos.x += paddle->vel.x * delta_time;
  paddle->pos.y += paddle->vel.y * delta_time;

  paddle->pos.y = std::clamp(paddle->pos.y, 0.0f, arena_height - paddle_height);
}

void updateGameState(GameState &state, double delta_time) {

  // clamp x direction
  state.p1.pos.x =
      std::clamp(state.p1.pos.x, 0.0f, arena_width / 2.0f - paddle_width);
  state.p2.pos.x =
      std::clamp(state.p2.pos.x, 0.0f, arena_width / 2.0f - paddle_width);
  state.p3.pos.x =
      std::clamp(state.p3.pos.x, (arena_width / 2.0f) + center_line_width,
                 arena_width - paddle_width);
  state.p4.pos.x =
      std::clamp(state.p4.pos.x, (arena_width / 2.0f) + center_line_width,
                 arena_width - paddle_width);

  // p1 side padle hit check
  if (state.ball.pos.x <= paddle_width + ball_radius &&
      state.ball.pos.y + ball_radius > state.p1.pos.y &&
      state.ball.pos.y - ball_radius < state.p1.pos.y + paddle_height) {

    float paddle_center_y = (state.p1.pos.y + (paddle_height / 2.0));
    float diff = state.ball.pos.y - paddle_center_y;
    float normalized_diff = diff / ((paddle_height / 2.0) + ball_radius);
    float angle = normalized_diff * (0.0174533 * 75.0);

    state.ball.vel.x = state.ball_speed * cos(angle);
    state.ball.vel.y = state.ball_speed * sin(angle);
  }

  // p2 side padle hit check
  if (state.ball.pos.x >= arena_width - (ball_radius + paddle_width) &&
      state.ball.pos.y + ball_radius > state.p2.pos.y &&
      state.ball.pos.y - ball_radius < state.p2.pos.y + paddle_height) {

    float paddle_center_y = (state.p2.pos.y + (paddle_height / 2.0));
    float diff = state.ball.pos.y - paddle_center_y;
    float normalized_diff = diff / ((paddle_height / 2.0) + ball_radius);
    float angle = normalized_diff * (0.0174533 * max_bounce_angle);

    state.ball.vel.x = -state.ball_speed * cos(angle);
    state.ball.vel.y = -state.ball_speed * sin(angle);
  }

  // if hitting top and bottom of screen, bounce
  if (state.ball.pos.y < 0) {
    state.ball.vel.y = -state.ball.vel.y;
  }

  if (state.ball.pos.y - ball_radius > arena_height) {
    state.ball.vel.y = -state.ball.vel.y;
  }

  // if hitting out of bounds, change score & reset game
  if (state.ball.pos.x < 0 && state.ball.vel.x < 0) {
    resetRound(state);
    state.p2_score++;
    state.ball_speed =
        std::min(state.ball_speed + ball_speed_inc, max_ball_speed);
  }

  if (state.ball.pos.x > arena_width && state.ball.vel.x > 0) {
    resetRound(state);
    state.p1_score++;
    state.ball_speed =
        std::min(state.ball_speed + ball_speed_inc, max_ball_speed);
  }

  // state.ball.pos.x += state.ball.vel.x * delta_time;
  // state.ball.pos.y += state.ball.vel.y * delta_time;
}
