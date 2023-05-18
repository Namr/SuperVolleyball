#include "inputs.hpp"
#include <capnp/serialize-packed.h>

kj::ArrayPtr<kj::byte> generateTestInput() {
  kj::VectorOutputStream stream;
  capnp::MallocMessageBuilder message;

  PlayerInputState::Builder input = message.initRoot<PlayerInputState>();
  input.setKeycombo(10);

  capnp::writePackedMessage(stream, message);
  return stream.getArray();
}
