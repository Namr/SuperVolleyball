#pragma once
#include "core/inputs.hpp"
#include "core/serialization.hpp"
#include "core/vectors.hpp"

#include <memory>
#include <mutex>
#include <array>
#include <optional>

namespace svb {

constexpr float PLAYER_SPEED = 300.0;
constexpr int max_players = 4;

class Player {
public:
  Player();
  Player(int game_position);
  void update(input::PlayerInputState input);
  void tick(float delta_time);

  inline const Vector2f &getPosition() const { return position_; };
  inline const Vector2f &getVelocity() const { return velocity_; };
  inline const float getRadius() const { return radius_; };

private:
  Vector2f position_;
  Vector2f velocity_;
  float radius_;
  std::mutex lock_;
};

class World : Serializable {
public:
  World() = default;
  kj::Array<capnp::word> serialize();

  std::array<std::optional<std::shared_ptr<Player>>, max_players> players;
};

} // namespace svb
