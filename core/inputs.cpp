#include "core/proto/input.capnp.h"
#include "inputs.hpp"

namespace svb_inputs {

void PlayerInputState::addKey(uint32_t key) { state_ = state_ | key; }

void PlayerInputState::clearKey(uint32_t key) { state_ = state_ | (~key); }

bool PlayerInputState::hasKey(uint32_t key) { return (state_ & key) == key; }

void PlayerInputState::reset() { state_ = 0; };

kj::Array<capnp::word> PlayerInputState::serialize() {
  capnp::MallocMessageBuilder message;
  auto serialized_input = message.initRoot<::PlayerInputState>();
  serialized_input.setState(state_);
  return capnp::messageToFlatArray(message);
}

void PlayerInputState::deserialize(const kj::ArrayPtr<capnp::word> &raw_data) {
  reset();

  capnp::FlatArrayMessageReader message(raw_data);
  auto input = message.getRoot<::PlayerInputState>();

  state_ = input.getState();
}

}; // namespace svb_inputs
