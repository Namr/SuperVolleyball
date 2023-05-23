#pragma once
#include "core/serialization.hpp"
#include <cstdint>

#include <capnp/message.h>
#include <capnp/serialize.h>

namespace svb_inputs {

constexpr uint32_t PLAYER_UP = 1;
constexpr uint32_t PLAYER_DOWN = 2;
constexpr uint32_t PLAYER_LEFT = 4;
constexpr uint32_t PLAYER_RIGHT = 8;
constexpr uint32_t PLAYER_JUMP = 16;
constexpr uint32_t TARGET_UP = 32;
constexpr uint32_t TARGET_DOWN = 64;
constexpr uint32_t TARGET_LEFT = 128;
constexpr uint32_t TARGET_RIGHT = 256;

class PlayerInputState : Serializable {
public:
  PlayerInputState() = default;
  void addKey(uint32_t key);
  void clearKey(uint32_t key);
  bool hasKey(uint32_t key);
  void reset();

  kj::Array<capnp::word> serialize() override; 
  void deserialize(const kj::ArrayPtr<capnp::word>& raw_data) override;
private:
  uint32_t state_ = 0;
};

};
