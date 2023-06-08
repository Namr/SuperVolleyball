#pragma once
#include "core/inputs.hpp"
#include "core/serialization.hpp"
#include "core/vectors.hpp"

#include <memory>
#include <mutex>
#include <array>
#include <optional>

namespace svb {

constexpr float player_speed = 300.0;
constexpr float player_base_radius = 24.0;
constexpr int max_players = 4;

constexpr int canvas_width = 1024;
constexpr int canvas_height = 576;
constexpr int court_center_x = 512;
constexpr int court_center_y = 288;
constexpr int court_padding_x = 100;
constexpr int court_padding_y = 50;

constexpr int court_width = (canvas_width - (court_padding_x * 2))/ 2;
constexpr int court_height = (canvas_height - (court_padding_y * 2)) / 2;

class Player {
public:
  Player();
  Player(int role);
  void update(input::PlayerInputState input);
  void tick(float delta_time);

  inline const Vector2f &getPosition() const { return position_; };
  inline const Vector2f &getVelocity() const { return velocity_; };
  inline const float getRadius() const { return radius_; };

private:
  Vector2f position_;
  Vector2f velocity_;
  float radius_;
  int role_;
  std::mutex lock_;
};

class World : Serializable {
public:
  World() = default;
  kj::Array<capnp::word> serialize();

  std::array<std::optional<std::shared_ptr<Player>>, max_players> players;
};

} // namespace svb
