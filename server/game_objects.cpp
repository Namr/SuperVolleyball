#include "server/game_objects.hpp"
#include "core/proto/entities.capnp.h"
#include <iostream>

namespace svb {
// TODO: sane position defaults? Constructor that takes "position" in match?
Player::Player() : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(35.0) {}

void Player::update(input::PlayerInputState input) {
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
}

void Player::tick() {
  position_ += velocity_;
  // std::cout << "x: " << position_.x() << " | y: " << position_.y() << std::endl;
}

kj::Array<capnp::word> World::serialize() {
  capnp::MallocMessageBuilder message;
  ::EntityList::Builder serialized_world = message.initRoot<::EntityList>();
  auto entities = serialized_world.initEntities(players.size());

  for(int i = 0; i < players.size(); i++) {
    ::Entity::Builder e = entities[i];
    std::shared_ptr<Player> p = players[i];
    e.getPosition().setX(p->getPosition().x());
    e.getPosition().setY(p->getPosition().y());
    e.getVelocity().setX(p->getVelocity().x());
    e.getVelocity().setY(p->getVelocity().y());
    e.setRadius(p->getRadius());
  }

  return capnp::messageToFlatArray(message);
}

} // namespace svb
