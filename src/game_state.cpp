#include "network_signals.hpp"

void resetGameState(GameState &state) {
  state.p1_paddle.vel.x = 0;
  state.p1_paddle.vel.y = 0;
  state.p1_paddle.pos.x = paddle_width;
  state.p1_paddle.pos.y = arena_height / 2.0;

  state.p2_paddle.vel.x = 0;
  state.p2_paddle.vel.y = 0;
  state.p2_paddle.pos.x = arena_width - paddle_width;
  state.p2_paddle.pos.y = arena_height / 2.0;

  state.ball.vel.x = 0;
  state.ball.vel.y = 0;
  state.ball.pos.x = arena_width / 2.0;
  state.ball.pos.y = arena_height / 2.0;

  state.p1_score = 0;
  state.p2_score = 0;
}

void updatePlayerState(GameState &state, const InputMessage &input,
                       uint8_t player) {
  PhysicsState &paddle = player == 0 ? state.p1_paddle : state.p2_paddle;
  if (input.up == true) {
    paddle.vel.y = -paddle_speed;
  }
  if (input.down == true) {
    paddle.vel.y = paddle_speed;
  }

  if ((input.up | input.down) == false) {
    paddle.vel.y = 0.0;
  }

  paddle.pos.x += paddle.vel.x * input.delta_time;
  paddle.pos.y += paddle.vel.y * input.delta_time;

  paddle.pos.x = std::clamp(paddle.pos.x, 0.0f, arena_width);
  paddle.pos.y = std::clamp(paddle.pos.y, 0.0f, arena_height);
}

void updateGameState(GameState &state, double delta_time) {}
