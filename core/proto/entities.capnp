@0xa0dbecffc6b10a8e;

struct Vector2f {
  x @0 :Float32;
  y @1 :Float32;
}

struct Entity {
  position @0 :Vector2f;
  velocity @1 :Vector2f;
  radius @2 :Float32;
}

struct EntityList {
  entities @0 :List(Entity);
}
