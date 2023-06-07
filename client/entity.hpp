#pragma once
#include "core/serialization.hpp"
#include "core/vectors.hpp"
#include <capnp/serialize.h>
#include <vector>

namespace svb {
class Entity {
public:
  Entity() = default;
  void render() const;
  void tick(float delta_time);

  Vector2f position;
  Vector2f velocity;
  float radius;
};

class EntityList : Deserializable {
public:
  std::vector<Entity> entities;
  void deserialize(const kj::ArrayPtr<capnp::word> &raw_data) override;
};

} // namespace svb
