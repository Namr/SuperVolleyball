#include "client/entity.hpp"
#include "core/proto/entities.capnp.h"
#include "raylib.h"

namespace svb {

void Entity::render() const {
  Color c;

  switch (color) {
  default:
  case 0:
    c = RED;
    break;
  case 1:
    c = GREEN;
    break;
  case 2:
    c = BLUE;
    break;
  }
  DrawCircle(position.x(), position.y(), radius, c);
}

void Entity::tick(float delta_time) { position += velocity * delta_time; }

void EntityList::deserialize(const kj::ArrayPtr<capnp::word> &raw_data) {
  entities.clear();
  capnp::FlatArrayMessageReader message(raw_data);
  auto list = message.getRoot<::EntityList>();

  size_t num_entities = list.getEntities().size();

  for (const ::Entity::Reader &e : list.getEntities()) {
    Entity to_add;
    to_add.position.x() = e.getPosition().getX();
    to_add.position.y() = e.getPosition().getY();
    to_add.velocity.x() = e.getVelocity().getX();
    to_add.velocity.y() = e.getVelocity().getY();
    to_add.radius = e.getRadius();
    to_add.color = e.getColor();
    entities.push_back(to_add);
  }
}
} // namespace svb
