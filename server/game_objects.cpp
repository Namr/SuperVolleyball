#include "server/game_objects.hpp"
#include <iostream>

namespace svb {
// TODO: sane position defaults? Constructor that takes "position" in match?
Player::Player() : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(10.0) {}

void Player::update(input::PlayerInputState input) {
  velocity_.reset();
  if (input.hasKey(input::PLAYER_UP)) {
    velocity_ += Vector2f(0.0, 1.0);
  }
  if (input.hasKey(input::PLAYER_DOWN)) {
    velocity_ += Vector2f(0.0, -1.0);
  }
  if (input.hasKey(input::PLAYER_LEFT)) {
    velocity_ += Vector2f(-1.0, 0.0);
  }
  if (input.hasKey(input::PLAYER_RIGHT)) {
    velocity_ += Vector2f(1.0, 0.0);
  }

  // normalize
  if (velocity_.normSquared() != 0.0) {
    velocity_ = velocity_ / velocity_.norm();
    velocity_ *= PLAYER_SPEED;
  }

  // TODO: implement targeting
  if (input.hasKey(input::TARGET_UP)) {
  }
  if (input.hasKey(input::TARGET_DOWN)) {
  }
  if (input.hasKey(input::TARGET_LEFT)) {
  }
  if (input.hasKey(input::TARGET_RIGHT)) {
  }
  if (input.hasKey(input::PLAYER_JUMP)) {
  }
}

void Player::tick() {
  position_ += velocity_;
  std::cout << "x: " << position_.x() << " | y: " << position_.y() << std::endl;
}

} // namespace svb
