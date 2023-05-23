#pragma once
#include <vector>

#include <capnp/message.h>
#include <capnp/serialize.h>

class Serializable {
  virtual kj::Array<capnp::word> serialize() = 0;
  virtual void deserialize(const kj::ArrayPtr<capnp::word>& raw_data) = 0;
};
