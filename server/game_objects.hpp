#pragma once
#include "core/court_definition.hpp"
#include "core/inputs.hpp"
#include "core/serialization.hpp"
#include "core/vectors.hpp"

#include <array>
#include <memory>
#include <mutex>
#include <optional>
#include <random>

namespace svb {

class Ball {
public:
  Ball();
  void tick(float delta_time);
  void chooseTarget();

  inline const Vector2f &getPosition() const { return position_; };
  inline const Vector2f &getTargetPosition() const { return target_; };
  inline const Vector2f &getVelocity() const { return velocity_; };
  inline const float getRadius() const { return radius_; };
  inline const int8_t getSide() const { return side_; };

private:
  Vector2f position_;
  Vector2f velocity_;
  Vector2f target_;
  int8_t side_;
  float radius_;
  std::mutex lock_;

  std::default_random_engine generator_;
  std::uniform_real_distribution<float> target_x_distribution_1_;
  std::uniform_real_distribution<float> target_x_distribution_2_;
  std::uniform_real_distribution<float> target_y_distribution_;
};

class Player {
public:
  Player();
  Player(int role);
  void update(input::PlayerInputState input);
  void tick(float delta_time);
  void onBallCollision(Ball &b);

  inline const Vector2f &getPosition() const { return position_; };
  inline const Vector2f &getVelocity() const { return velocity_; };
  inline const float getRadius() const { return radius_; };

private:
  Vector2f position_;
  Vector2f velocity_;
  float radius_;
  int role_;
  bool hitting_;
  std::mutex lock_;
};

class World : Serializable {
public:
  World() = default;
  kj::Array<capnp::word> serialize();

  std::array<std::optional<std::shared_ptr<Player>>, max_players> players;
  Ball ball;
};

} // namespace svb
