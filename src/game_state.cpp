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
  ret.p1_paddle = interpolate(previous.p1_paddle, next.p1_paddle, a);
  ret.p2_paddle = interpolate(previous.p2_paddle, next.p2_paddle, a);
  ret.ball = interpolate(previous.ball, next.ball, a);

  // things that should just snap to previous
  ret.ball_speed = previous.ball_speed;
  ret.p1_score = previous.p1_score;
  ret.p2_score = previous.p2_score;
  ret.tick = previous.tick;
  return ret;
}
void resetGameState(GameState &state) {
  state.p1_paddle.vel.x = 0;
  state.p1_paddle.vel.y = 0;
  state.p1_paddle.pos.x = 0;
  state.p1_paddle.pos.y = arena_height / 2.0;

  state.p2_paddle.vel.x = 0;
  state.p2_paddle.vel.y = 0;
  state.p2_paddle.pos.x = arena_width - paddle_width;
  state.p2_paddle.pos.y = arena_height / 2.0;

  state.ball_speed = init_ball_speed;
  state.ball.vel.x = -state.ball_speed;
  state.ball.vel.y = 0;
  state.ball.pos.x = arena_width / 2.0;
  state.ball.pos.y = arena_height / 2.0;

  state.p1_score = 0;
  state.p2_score = 0;
}

void resetRound(GameState &state) {
  state.p1_paddle.vel.x = 0;
  state.p1_paddle.vel.y = 0;
  state.p1_paddle.pos.x = 0;
  state.p1_paddle.pos.y = arena_height / 2.0;

  state.p2_paddle.vel.x = 0;
  state.p2_paddle.vel.y = 0;
  state.p2_paddle.pos.x = arena_width - paddle_width;
  state.p2_paddle.pos.y = arena_height / 2.0;

  state.ball.vel.x = -state.ball_speed;
  state.ball.vel.y = 0;
  state.ball.pos.x = arena_width / 2.0;
  state.ball.pos.y = arena_height / 2.0;
}

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player) {
  PhysicsState &paddle = player == 0 ? state.p1_paddle : state.p2_paddle;
  if (input.up) {
    paddle.vel.y = -paddle_speed;
  }
  if (input.down) {
    paddle.vel.y = paddle_speed;
  }

  if ((input.up | input.down) == false) {
    paddle.vel.y = 0.0;
  }

  paddle.pos.x += paddle.vel.x * delta_time;
  paddle.pos.y += paddle.vel.y * delta_time;

  paddle.pos.x = std::clamp(paddle.pos.x, 0.0f, arena_width);
  paddle.pos.y = std::clamp(paddle.pos.y, 0.0f, arena_height - paddle_height);
}

void updateGameState(GameState &state, double delta_time) {

  // p1 side padle hit check
  if (state.ball.pos.x <= paddle_width + ball_radius &&
      state.ball.pos.y + ball_radius > state.p1_paddle.pos.y &&
      state.ball.pos.y - ball_radius < state.p1_paddle.pos.y + paddle_height) {

    float paddle_center_y = (state.p1_paddle.pos.y + (paddle_height / 2.0));
    float diff = state.ball.pos.y - paddle_center_y;
    float normalized_diff = diff / ((paddle_height / 2.0) + ball_radius);
    float angle = normalized_diff * (0.0174533 * 75.0);

    state.ball.vel.x = state.ball_speed * cos(angle);
    state.ball.vel.y = state.ball_speed * sin(angle);
  }

  // p2 side padle hit check
  if (state.ball.pos.x >= arena_width - (ball_radius + paddle_width) &&
      state.ball.pos.y + ball_radius > state.p2_paddle.pos.y &&
      state.ball.pos.y - ball_radius < state.p2_paddle.pos.y + paddle_height) {

    float paddle_center_y = (state.p2_paddle.pos.y + (paddle_height / 2.0));
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

  state.ball.pos.x += state.ball.vel.x * delta_time;
  state.ball.pos.y += state.ball.vel.y * delta_time;
}
