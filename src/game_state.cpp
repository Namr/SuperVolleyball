#include "game_state.hpp"
#include <algorithm>
#include <math.h>
#include <random>

Vec3 interpolate(Vec3 &previous, Vec3 &next, double a) {
  Vec3 ret;
  ret.x = next.x * a + previous.x * (1.0 - a);
  ret.y = next.y * a + previous.y * (1.0 - a);
  ret.z = next.z * a + previous.z * (1.0 - a);
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
  ret.target = interpolate(previous.target, next.target, a);
  ret.landing_zone = interpolate(previous.landing_zone, next.landing_zone, a);

  ret.timer = next.timer * a + previous.timer * (1.0 - a);

  // things that should just snap to previous
  ret.team1_score = previous.team1_score;
  ret.team2_score = previous.team2_score;
  ret.team1_points_to_give = previous.team1_points_to_give;
  ret.team2_points_to_give = previous.team2_points_to_give;
  ret.tick = previous.tick;

  ret.ball_state = previous.ball_state;
  ret.last_server = previous.last_server;
  ret.ball_owner = previous.ball_owner;
  ret.can_owner_move = previous.can_owner_move;
  return ret;
}

PhysicsState *playerFromIndex(GameState &state, int idx) {
  PhysicsState *paddle = nullptr;
  switch (idx) {
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
  return paddle;
}

// speed = z_distance / desired_time
// desired_time = xy_distance / xy_speed
// speed = state.ball_state.z / (xy_distance / xy_speed)
void sendBallDownToTarget(GameState &state, const Vec3 &target,
                          const float speed) {
  Vec3 to_target = state.ball.pos - target;
  float magnitude = to_target.magnitude2D();
  if (magnitude > 0.01) {
    to_target.x /= magnitude;
    to_target.y /= magnitude;
  }
  to_target *= speed;
  to_target.z = -state.ball.pos.z / (magnitude / speed);
  state.ball.vel = to_target;
}

void passBallToTarget(GameState &state, const Vec3 &target) {
  Vec3 to_target = state.ball.pos - target;
  float magnitude = to_target.magnitude2D();
  if (magnitude > 0.01) {
    to_target.x /= magnitude;
    to_target.y /= magnitude;
  }
  float desired_xy_speed = magnitude / ball_passing_time;
  float desired_z_speed = (ball_max_passing_height / ball_passing_time) * 2.0;
  to_target.x *= desired_xy_speed;
  to_target.y *= desired_xy_speed;
  to_target.z = desired_z_speed;
  state.ball.pos.z = 0.01;
  state.ball.vel = to_target;
}

void sendBallUpToTarget(GameState &state, const Vec3 &target,
                        const float speed) {
  Vec3 to_target = state.ball.pos - target;
  float magnitude = to_target.magnitude2D();
  if (magnitude > 0.01) {
    to_target.x /= magnitude;
    to_target.y /= magnitude;
  }
  to_target *= speed;
  to_target.z = (ball_max_passing_height / (magnitude / speed)) * 2.0;
  state.ball.pos.z = 0.01;
  state.ball.vel = to_target;
}

Vec3 centerOfOpposingCourt(int idx) {
  Vec3 ret;
  ret.z = 0;
  ret.y = arena_height / 2.0;
  switch (idx) {
  case 0:
  case 1:
    ret.x = 3.0f * arena_width / 4.0f;
    break;
  case 2:
  case 3:
  default:
    ret.x = arena_width / 4.0f;
    break;
  }
  return ret;
}

uint32_t getTeammateIdx(int player_idx) {
  switch (player_idx) {
  case 0:
    return 1;
  case 1:
    return 0;
  case 2:
    return 3;
  default:
  case 3:
    return 2;
  }
}

void giveOpponentPoints(GameState &state, int points, int player_idx) {
  if (player_idx == 0 || player_idx == 1) {
    state.team2_points_to_give += points;
  } else {
    state.team1_points_to_give += points;
  }
}

void givePlayerPoints(GameState &state, int points, int player_idx) {
  if (player_idx == 0 || player_idx == 1) {
    state.team1_points_to_give += points;
  } else {
    state.team2_points_to_give += points;
  }
}

Vec3 movePositionRandomly(const Vec3 &pos, float min, float max, uint32_t tick,
                          int player_idx) {
  auto rand = std::mt19937_64();
  rand.seed(tick);
  float dx = (std::generate_canonical<float, 128>(rand) *
              (passing_max_dist - passing_min_dist)) +
             passing_min_dist;
  float dy = std::generate_canonical<float, 128>(rand) * passing_max_dist;
  Vec3 pass_target = pos;
  pass_target.x += dx;
  pass_target.y += dy;

  if (player_idx == 0 || player_idx == 1) {
    pass_target.x =
        std::clamp(pass_target.x, 0.0f, arena_width / 2.0f - paddle_width);
  } else {
    pass_target.x =
        std::clamp(pass_target.x, (arena_width / 2.0f) + center_line_width,
                   arena_width - paddle_width);
  }
  pass_target.y = std::clamp(pass_target.y, 0.0f, arena_height - paddle_height);
  return pass_target;
}

bool playerBallInCollision(const Vec3 &ball_pos, const Vec3 &player_pos) {
  return (ball_pos - player_pos).magnitude2D() <
         (ball_radius + (ball_pos.z * Z_TO_SIZE_RATIO)) + hit_leeway +
             paddle_width;
}

bool playerCanReachUpToBall(const Vec3 &ball_pos, const Vec3 &player_pos) {
  return std::abs(ball_pos.z - player_pos.z) < hitting_max_z_dist;
}

void resetGameState(GameState &state) {
  state.p1.vel.x = 0;
  state.p1.vel.y = 0;
  state.p1.vel.z = 0;
  state.p1.pos.x = starting_dist_from_screen;
  state.p1.pos.y = arena_height / 4.0;
  state.p1.pos.z = 0;

  state.p2.vel.x = 0;
  state.p2.vel.y = 0;
  state.p2.vel.z = 0;
  state.p2.pos.x = starting_dist_from_screen;
  state.p2.pos.y = 3.0f * (arena_height / 4.0);
  state.p2.pos.z = 0;

  state.p3.vel.x = 0;
  state.p3.vel.y = 0;
  state.p3.vel.z = 0;
  state.p3.pos.x = arena_width - paddle_width - starting_dist_from_screen;
  state.p3.pos.y = arena_height / 4.0;
  state.p3.pos.z = 0;

  state.p4.vel.x = 0;
  state.p4.vel.y = 0;
  state.p4.vel.z = 0;
  state.p4.pos.x = arena_width - paddle_width - starting_dist_from_screen;
  state.p4.pos.y = 3.0f * (arena_height / 4.0);
  state.p4.pos.z = 0;

  state.target.vel.x = 0;
  state.target.vel.y = 0;
  state.target.vel.z = 0;
  state.target.pos.x = 0;
  state.target.pos.y = 0;
  state.target.pos.z = 0;

  state.landing_zone.vel.x = 0;
  state.landing_zone.vel.y = 0;
  state.landing_zone.vel.z = 0;
  state.landing_zone.pos.x = 0;
  state.landing_zone.pos.y = 0;
  state.landing_zone.pos.z = 0;

  state.ball_state = BALL_STATE_READY_TO_SERVE;
  state.last_server = 1;
  state.ball_owner = 1;
  state.can_owner_move = false;

  PhysicsState *owning_player = playerFromIndex(state, state.ball_owner - 1);
  state.ball.vel.x = 0;
  state.ball.vel.y = 0;
  state.ball.vel.z = 0;
  state.ball.pos.x = owning_player->pos.x + paddle_width;
  state.ball.pos.y = owning_player->pos.y + (paddle_height / 2.0);
  state.ball.pos.z = 0;

  state.timer = 0.0;

  state.team1_score = 0;
  state.team2_score = 0;
  state.team1_points_to_give = 0;
  state.team2_points_to_give = 0;
}

void resetRound(GameState &state) {
  state.p1.vel.x = 0;
  state.p1.vel.y = 0;
  state.p1.vel.z = 0;
  state.p1.pos.x = starting_dist_from_screen;
  state.p1.pos.y = arena_height / 4.0;
  state.p1.pos.z = 0;

  state.p2.vel.x = 0;
  state.p2.vel.y = 0;
  state.p2.vel.z = 0;
  state.p2.pos.x = starting_dist_from_screen;
  state.p2.pos.y = 3.0f * (arena_height / 4.0);
  state.p2.pos.z = 0;

  state.p3.vel.x = 0;
  state.p3.vel.y = 0;
  state.p3.vel.z = 0;
  state.p3.pos.x = arena_width - paddle_width - starting_dist_from_screen;
  state.p3.pos.y = arena_height / 4.0;
  state.p3.pos.z = 0;

  state.p4.vel.x = 0;
  state.p4.vel.y = 0;
  state.p4.vel.z = 0;
  state.p4.pos.x = arena_width - paddle_width - starting_dist_from_screen;
  state.p4.pos.y = 3.0f * (arena_height / 4.0);
  state.p4.pos.z = 0;

  state.target.vel.x = 0;
  state.target.vel.y = 0;
  state.target.vel.z = 0;
  state.target.pos.x = 0;
  state.target.pos.y = 0;
  state.target.pos.z = 0;

  state.landing_zone.vel.x = 0;
  state.landing_zone.vel.y = 0;
  state.landing_zone.vel.z = 0;
  state.landing_zone.pos.x = 0;
  state.landing_zone.pos.y = 0;
  state.landing_zone.pos.z = 0;

  state.ball_state = BALL_STATE_READY_TO_SERVE;
  // FIXME: correct service rotation rules
  state.last_server += 1;
  if (state.last_server > 3) {
    state.last_server = 1;
  }
  state.ball_owner = state.last_server;
  state.can_owner_move = false;

  PhysicsState *owning_player = playerFromIndex(state, state.ball_owner - 1);
  state.ball.vel.x = 0;
  state.ball.vel.y = 0;
  state.ball.vel.z = 0;
  state.ball.pos.x = owning_player->pos.x + paddle_width;
  state.ball.pos.y = owning_player->pos.y + paddle_height;
  state.ball.pos.z = 0;

  state.timer = 0.0;

  state.team1_score += state.team1_points_to_give;
  state.team2_score += state.team2_points_to_give;
  state.team1_points_to_give = 0;
  state.team2_points_to_give = 0;
}

void updatePlayerState(GameState &state, const InputMessage &input,
                       const double delta_time, uint8_t player) {
  PhysicsState *paddle = playerFromIndex(state, player);
  bool is_owner = state.ball_owner == player + 1;

  // movement code
  if (!is_owner || state.can_owner_move) {
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

    if (paddle->pos.z >= jump_height) {
      paddle->vel.z = -jump_speed / 1.5;
    }

    if (input.jump && paddle->pos.z == 0.0) {
      paddle->vel.z = jump_speed;
    }

    // normalize velocity
    float magnitude = paddle->vel.magnitude2D();
    if (magnitude > 0.01) {
      paddle->vel.x = (paddle->vel.x / magnitude) * paddle_speed;
      paddle->vel.y = (paddle->vel.y / magnitude) * paddle_speed;
    }

    paddle->pos.x += paddle->vel.x * delta_time;
    paddle->pos.y += paddle->vel.y * delta_time;

    paddle->pos.y =
        std::clamp(paddle->pos.y, 0.0f, arena_height - paddle_height);
  }

  // target movement code
  if (is_owner && (state.ball_state == BALL_STATE_IN_SERVICE ||
                   state.ball_state == BALL_STATE_SECOND_PASS)) {

    if (input.target_up) {
      state.target.vel.y = -target_speed;
    }
    if (input.target_down) {
      state.target.vel.y = target_speed;
    }

    if ((input.target_up | input.target_down) == false) {
      state.target.vel.y = 0.0;
    }

    if (input.target_left) {
      state.target.vel.x = -target_speed;
    }
    if (input.target_right) {
      state.target.vel.x = target_speed;
    }

    if ((input.target_left | input.target_right) == false) {
      state.target.vel.x = 0.0;
    }

    // normalize velocity
    float magnitude = std::sqrt((state.target.vel.x * state.target.vel.x) +
                                (state.target.vel.y * state.target.vel.y));
    if (magnitude > 0.01) {
      state.target.vel.x = (state.target.vel.x / magnitude) * target_speed;
      state.target.vel.y = (state.target.vel.y / magnitude) * target_speed;
    }

    state.target.pos.x += state.target.vel.x * delta_time;
    state.target.pos.y += state.target.vel.y * delta_time;

    if (player == 0 || player == 1) {
      state.target.pos.x = std::clamp(state.target.pos.x,
                                      (arena_width / 2.0f) + center_line_width,
                                      arena_width - paddle_width);
    } else {
      state.target.pos.x = std::clamp(state.target.pos.x, 0.0f,
                                      arena_width / 2.0f - paddle_width);
    }
    state.target.pos.y =
        std::clamp(state.target.pos.y, 0.0f, arena_height - target_radius);
    state.target.pos.z = std::max(0.0f, state.target.pos.z);
  }

  // ball_owner is 1 indexed and 0 is N/A; player is 0  indexed
  if (is_owner) {
    // OWNER BALL STATE MACHINE
    if (state.ball_state == BALL_STATE_READY_TO_SERVE) {
      // start serve
      if (input.jump) {
        state.ball_state = BALL_STATE_IN_SERVICE;

        // lock the ball to the player
        state.ball.pos.x = paddle->pos.x + paddle_width;
        state.ball.pos.y = paddle->pos.y + (paddle_height / 2.0);

        // move the ball and player up
        paddle->vel.z = ball_up_speed;
        state.ball.vel.z = ball_up_speed;

        // move the target to the center of the opposing court
        state.target.pos = centerOfOpposingCourt(player);
      }
    } else if (state.ball_state == BALL_STATE_IN_SERVICE) {
      // hit serve to the other side
      if (state.timer > service_hittable_time && input.hit) {
        state.ball_state = BALL_STATE_TRAVELLING;
        state.ball_owner = -player; // negative values denote prev owner
        state.can_owner_move = true;
        state.landing_zone.pos = state.target.pos;
        sendBallDownToTarget(state, state.target.pos, ball_serving_speed);
        paddle->vel.z = -2 * ball_up_speed;
      }
    } else if (state.ball_state == BALL_STATE_FIRST_PASS) {
      if (playerBallInCollision(state.ball.pos, paddle->pos) &&
          playerCanReachUpToBall(state.ball.pos, paddle->pos) && input.hit) {
        // hit to your teammate again
        uint32_t teammate_idx = getTeammateIdx(player);
        PhysicsState *teammate = playerFromIndex(state, teammate_idx);
        state.ball_state = BALL_STATE_SECOND_PASS;
        state.ball_owner = teammate_idx + 1; // ball_owner is 1 indexed
        // add randomness to the pass
        Vec3 pass_target =
            movePositionRandomly(teammate->pos, passing_min_dist,
                                 passing_max_dist, state.tick, player);
        passBallToTarget(state, pass_target);
        state.landing_zone.pos = pass_target;

        // let your teammate aim
        state.target.pos = centerOfOpposingCourt(player);
      }
    } else if (state.ball_state == BALL_STATE_SECOND_PASS) {
      if (playerBallInCollision(state.ball.pos, paddle->pos) && input.hit) {
        if (paddle->pos.z > spiking_min_player_z) {
          state.ball_state = BALL_STATE_TRAVELLING;
          state.ball_owner = -player; // negative values denote prev owner
          state.landing_zone.pos = state.target.pos;
          sendBallDownToTarget(state, state.target.pos, ball_spiking_speed);
        } else if (playerCanReachUpToBall(state.ball.pos, paddle->pos)) {
          state.ball_state = BALL_STATE_TRAVELLING;
          state.ball_owner = -player; // negative values denote prev owner
          state.landing_zone.pos = state.target.pos;
          sendBallUpToTarget(state, state.target.pos, ball_shooting_speed);
        }
      }
    }
  } // END ball owner logic
  else {
    if (state.ball_state == BALL_STATE_TRAVELLING) {
      // let the player pass the ball to their team-mate
      // TODO: mechanism for blocking
      if (state.ball.vel.z > 0 && state.ball.pos.z >= ball_max_passing_height) {
        state.ball.vel.z *= -1;
      }

      if (playerBallInCollision(state.ball.pos, paddle->pos) && input.hit &&
          state.ball_owner != -player &&
          state.ball_owner != -getTeammateIdx(player)) {
        uint32_t teammate_idx = getTeammateIdx(player);
        state.ball_state = BALL_STATE_FIRST_PASS;
        state.ball_owner = teammate_idx + 1; // ball_owner is 1 indexed...

        PhysicsState *teammate = playerFromIndex(state, teammate_idx);
        // add some randomness to this pass
        Vec3 pass_target =
            movePositionRandomly(teammate->pos, passing_min_dist,
                                 passing_max_dist, state.tick, player);
        passBallToTarget(state, pass_target);
        state.landing_zone.pos = pass_target;
      }
    }
  } // END NON-OWNER LOGIC
  paddle->pos.z += paddle->vel.z * delta_time;
  paddle->pos.z = std::max(0.0f, paddle->pos.z);
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

  // BALL STATE MACHINE
  if (state.ball_state == BALL_STATE_IN_SERVICE) {
    PhysicsState *owning_player = playerFromIndex(state, state.ball_owner - 1);
    state.timer += delta_time;

    // if timer is too large, you fail service
    if (state.timer > service_max_time) {
      state.ball_state = BALL_STATE_FAILED_SERVICE;
    }
  } else if (state.ball_state == BALL_STATE_FAILED_SERVICE) {
    // let the ball drop back down & then reset
    PhysicsState *owning_player = playerFromIndex(state, state.ball_owner - 1);
    state.timer = 0;
    owning_player->vel.z = -2 * ball_up_speed;
    state.ball.vel.z = -2 * ball_up_speed;
    if (state.ball.pos.z <= 0) {
      // make the owner lose a point
      giveOpponentPoints(state, 1, state.ball_owner - 1);
      state.ball_state = BALL_STATE_GAME_OVER;
      state.timer = 0;
    }
  } else if (state.ball_state == BALL_STATE_TRAVELLING) {
    // if we get to the target make the loser lose
    if ((state.ball.pos - state.target.pos).magnitude2D() < target_radius) {
      // negative values of ball_owner denote previous owner
      givePlayerPoints(state, 1, -state.ball_owner);
      state.ball_state = BALL_STATE_GAME_OVER;
      state.timer = 0;
    }
  } else if (state.ball_state == BALL_STATE_FIRST_PASS ||
             state.ball_state == BALL_STATE_SECOND_PASS) {
    if (state.ball.vel.z > 0 && state.ball.pos.z >= ball_max_passing_height) {
      // go down
      state.ball.vel.z *= -0.95;
    } else {
      // if we hit the ground when passing, game over
      if (state.ball.pos.z <= 0) {
        // make the owner lose a point
        giveOpponentPoints(state, 1, state.ball_owner - 1);
        state.ball_state = BALL_STATE_GAME_OVER;
        state.timer = 0;
      }
    }
  } else if (state.ball_state == BALL_STATE_GAME_OVER) {
    state.timer += delta_time;
    if (state.timer > game_over_grace_period) {
      resetRound(state);
    }
  }

  state.ball.pos.x += state.ball.vel.x * delta_time;
  state.ball.pos.y += state.ball.vel.y * delta_time;
  state.ball.pos.z += state.ball.vel.z * delta_time;
  state.ball.pos.z = std::max(state.ball.pos.z, 0.0f);
}
