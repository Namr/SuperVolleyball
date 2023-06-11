#include "core/proto/entities.capnp.h"
#include "server/game_objects.hpp"
#include <iostream>

namespace svb {

Player::Player()
    : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(player_base_radius),
      hitting_(false) {}

Player::Player(int game_position)
    : position_(0.0, 0.0), velocity_(0.0, 0.0), radius_(player_base_radius),
      hitting_(false) {

  switch (game_position) {
  case 0:
    position_ =
        Vector2f(court_center_x - court_width, court_center_y - court_height);
    break;
  case 1:
    position_ =
        Vector2f(court_center_x - court_width, court_center_y + court_height);
    break;
  case 2:
    position_ =
        Vector2f(court_center_x + court_width, court_center_y - court_height);
    break;
  case 3:
    position_ =
        Vector2f(court_center_x + court_width, court_center_y + court_height);
    break;
  default:
    std::cout << "ERROR: invalid game position given, this should never happen"
              << std::endl;
    break;
  }

  role_ = game_position;
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
    velocity_ *= player_speed;
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
    hitting_ = true;
  } else {
    hitting_ = false;
  }
  lock_.unlock();
}

void Player::tick(float delta_time) {
  lock_.lock();
  position_ += velocity_ * delta_time;

  // enforce court bounds
  if (position_.y() > court_center_y + court_height) {
    position_.y() = court_center_y + court_height;
  }
  if (position_.y() < court_center_y - court_height) {
    position_.y() = court_center_y - court_height;
  }

  if (role_ <= 1) {
    if (position_.x() > court_center_x) {
      position_.x() = court_center_x;
    }
    if (position_.x() < court_center_x - court_width) {
      position_.x() = court_center_x - court_width;
    }
  } else {
    if (position_.x() < court_center_x) {
      position_.x() = court_center_x;
    }
    if (position_.x() > court_center_x + court_width) {
      position_.x() = court_center_x + court_width;
    }
  }

  lock_.unlock();
}

void Player::onBallCollision(Ball &ball) {
  int8_t side = role_ <= 1 ? 1 : -1;
  if (hitting_ && side == ball.getSide()) {
    ball.chooseTarget();
  }
}

Ball::Ball()
    : position_(court_center_x, court_center_y), velocity_(0.0, 0.0),
      target_(0.0, 0.0), radius_(ball_base_radius), side_(-1),
      target_x_distribution_1_(court_center_x - court_width,
                               court_center_x - ball_base_radius - 20),
      target_x_distribution_2_(court_center_x + ball_base_radius + 20,
                               court_center_x + court_width),
      target_y_distribution_(court_padding_y,
                             court_padding_y + (court_height * 2)) {
  chooseTarget();
}

void Ball::chooseTarget() {
  side_ *= -1;

  if (side_ == 1) {
    target_ = {target_x_distribution_1_(generator_),
               target_y_distribution_(generator_)};
  } else {
    target_ = {target_x_distribution_2_(generator_),
               target_y_distribution_(generator_)};
  }
}

void Ball::tick(float delta_time) {
  lock_.lock();
  Vector2f direction = target_ - position_;
  // normalize
  if (direction.normSquared() != 0.0) {
    direction = direction / direction.norm();
    velocity_ = direction * ball_speed;
  } else {
    velocity_ = {0.0, 0.0};
  }

  if ((position_ - target_).norm() < 4.0) {
    // chooseTarget();
    velocity_ = {0.0, 0.0};
  }

  position_ += velocity_ * delta_time;

  lock_.unlock();
}

kj::Array<capnp::word> World::serialize() {
  capnp::MallocMessageBuilder message;
  ::EntityList::Builder serialized_world = message.initRoot<::EntityList>();
  auto entities = serialized_world.initEntities(players.size() + 2);

  for (int i = 0; i < svb::max_players; i++) {
    if (players[i]) {
      ::Entity::Builder e = entities[i];
      std::shared_ptr<Player> p = players[i].value();
      e.getPosition().setX(p->getPosition().x());
      e.getPosition().setY(p->getPosition().y());
      e.getVelocity().setX(p->getVelocity().x());
      e.getVelocity().setY(p->getVelocity().y());
      e.setRadius(p->getRadius());
      e.setColor(0);
    }
  }

  ::Entity::Builder t = entities[players.size()];
  t.getPosition().setX(ball.getTargetPosition().x());
  t.getPosition().setY(ball.getTargetPosition().y());
  t.getVelocity().setX(0.0);
  t.getVelocity().setY(0.0);
  t.setRadius(ball.getRadius());
  t.setColor(2);

  ::Entity::Builder b = entities[players.size() + 1];
  b.getPosition().setX(ball.getPosition().x());
  b.getPosition().setY(ball.getPosition().y());
  b.getVelocity().setX(ball.getVelocity().x());
  b.getVelocity().setY(ball.getVelocity().y());
  b.setRadius(ball.getRadius());
  b.setColor(1);

  return capnp::messageToFlatArray(message);
}

} // namespace svb
