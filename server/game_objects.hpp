#pragma once
#include "core/inputs.hpp"
#include "core/serialization.hpp"
#include "core/vectors.hpp"

#include <memory>
#include <vector>

namespace svb {

constexpr float PLAYER_SPEED = 3.0;

class Player {
public:
  Player();
  void update(input::PlayerInputState input);
  void tick();

  inline const Vector2f &getPosition() const { return position_; };
  inline const Vector2f &getVelocity() const { return velocity_; };
  inline const float getRadius() const { return radius_; };

private:
  Vector2f position_;
  Vector2f velocity_;
  float radius_;
};

class World : Serializable {
public:
  World() = default;
  kj::Array<capnp::word> serialize();

  std::vector<std::shared_ptr<Player>> players;
};

} // namespace svb
