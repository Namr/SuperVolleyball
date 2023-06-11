#pragma once
#include <math.h>

namespace svb {
class Vector2f {
public:
  Vector2f() : x_(0.0), y_(0.0){};
  Vector2f(float x, float y) : x_(x), y_(y){};
  inline float normSquared() const { return x_ * x_ + y_ * y_; };
  inline float norm() const { return sqrt(normSquared()); };

  inline void reset() {
    x_ = 0.0;
    y_ = 0.0;
  }

  inline Vector2f &operator+=(const Vector2f &other) {
    x_ += other.x();
    y_ += other.y();
    return *this;
  }

  inline Vector2f operator+(const Vector2f &other) const {
    Vector2f sum = *this; // should produce a copy
    sum += other;
    return sum;
  }

  inline Vector2f &operator-=(const Vector2f &other) {
    x_ -= other.x();
    y_ -= other.y();
    return *this;
  }

  inline Vector2f operator-(const Vector2f &other) const {
    Vector2f sum = *this; // should produce a copy
    sum -= other;
    return sum;
  }

  inline Vector2f &operator/=(const float other) {
    x_ /= other;
    y_ /= other;
    return *this;
  }

  inline Vector2f operator/(const float other) const {
    Vector2f total = *this; // should produce a copy
    total /= other;
    return total;
  }

  inline Vector2f &operator*=(const float other) {
    x_ *= other;
    y_ *= other;
    return *this;
  }

  inline Vector2f operator*(const float other) const {
    Vector2f total = *this; // should produce a copy
    total *= other;
    return total;
  }

  inline float x() const { return x_; };
  inline float y() const { return y_; };
  inline float &x() { return x_; };
  inline float &y() { return y_; };

private:
  float x_;
  float y_;
};
} // namespace svb
