#include "core/proto/entities.capnp.h"
#include "server/game_objects.hpp"
#include <iostream>

namespace svb {

constexpr int court_center_x = 512;
constexpr int court_center_y = 288;
constexpr int court_width = 624;
constexpr int court_height = 376;

Player::Player() : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(35.0) {}

Player::Player(int game_position)
    : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(35.0) {

  switch (game_position) {
  case 0:
    position_ = Vector2f(court_center_x - (court_width / 2),
                         court_center_y - (court_height / 2));
    break;
  case 1:
    position_ = Vector2f(court_center_x - (court_width / 2),
                         court_center_y + (court_height / 2));
    break;
  case 2:
    position_ = Vector2f(court_center_x + (court_width / 2),
                         court_center_y - (court_height / 2));
    break;
  case 3:
    position_ = Vector2f(court_center_x + (court_width / 2),
                         court_center_y + (court_height / 2));
    break;
  default:
    std::cout << "ERROR: invalid game position given, this should never happen"
              << std::endl;
    break;
  }
}

void Player::update(input::PlayerInputState input) {
  lock_.lock();
  velocity_.reset();
  if (input.hasKey(input::PLAYER_UP)) {
    velocity_ += Vector2f(0.0, -1.0);
  }
  if (input.hasKey(input::PLAYER_DOWN)) {
    velocity_ += Vector2f(0.0, 1.0);
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
  lock_.unlock();
}

void Player::tick(float delta_time) {
  lock_.lock();
  position_ += velocity_ * delta_time;
  // std::cout << "x: " << position_.x() << " | y: " << position_.y() <<
  // std::endl;
  lock_.unlock();
}

kj::Array<capnp::word> World::serialize() {
  capnp::MallocMessageBuilder message;
  ::EntityList::Builder serialized_world = message.initRoot<::EntityList>();
  auto entities = serialized_world.initEntities(players.size());

  for (int i = 0; i < svb::max_players; i++) {
    if (players[i]) {
      ::Entity::Builder e = entities[i];
      std::shared_ptr<Player> p = players[i].value();
      e.getPosition().setX(p->getPosition().x());
      e.getPosition().setY(p->getPosition().y());
      e.getVelocity().setX(p->getVelocity().x());
      e.getVelocity().setY(p->getVelocity().y());
      e.setRadius(p->getRadius());
    }
  }

  return capnp::messageToFlatArray(message);
}

} // namespace svb
