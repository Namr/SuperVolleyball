#pragma once
#include "core/inputs.hpp"
#include "core/vectors.hpp"

#include <vector>
#include <memory>

namespace svb {

constexpr float PLAYER_SPEED = 5.0;

class Player {
  public:
    Player();
    void update(input::PlayerInputState input);
    void tick();
  private:
    Vector2f position_;
    Vector2f velocity_;
    float radius_;
};

struct World {
  std::vector<std::shared_ptr<Player>> players;
};

} // namespace svb
