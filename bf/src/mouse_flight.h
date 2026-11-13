#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace bf1943::controls {

// Relative aircraft mouse input is a rate, sampled once per displayed frame.
// Re-reading the gamepad must not re-consume a mouse delta or change its scale.
class MouseFlight {
 public:
  void Reset() { x_ = y_ = idle_ = 0; }

  void Update(double dx, double dy, double seconds, double sensitivity) {
    if (!std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(seconds) ||
        !std::isfinite(sensitivity) || seconds <= 0 || seconds > 0.25) {
      Reset();
      return;
    }
    sensitivity = std::clamp(sensitivity, 0.05, 10.0);
    // A deliberate sweep at 1,000 raw counts/s reaches full deflection at 1x.
    const double scale = sensitivity / (1000.0 * seconds);
    const double wanted_x = std::clamp(dx * scale, -1.0, 1.0);
    const double wanted_y = std::clamp(-dy * scale, -1.0, 1.0);
    const double alpha = -std::expm1(-seconds / 0.025);
    x_ += (wanted_x - x_) * alpha;
    y_ += (wanted_y - y_) * alpha;
    idle_ = dx == 0 && dy == 0 ? idle_ + seconds : 0;
    // A stopped mouse must not leave a persistent steering target.
    if (idle_ >= 0.10) x_ = y_ = 0;
  }

  double X() const { return x_; }
  double Y(bool inverted = false) const { return inverted ? -y_ : y_; }

  static int16_t GameAxis(double value) {
    if (!std::isfinite(value) || std::fabs(value) < 0.0001) return 0;
    value = std::clamp(value, -1.0, 1.0);
    // The pad mapping removes 15%, then the vehicle concept removes another
    // 25% of that range. Invert both stages once: .15 + .85 * (.25 + .75*v).
    // Without the second stage, gentle sweeps never reach the control surfaces.
    constexpr double dead_zone = 0.15 + 0.85 * 0.25;
    const double sent = std::copysign(dead_zone + std::fabs(value) * (1.0 - dead_zone), value);
    return int16_t(std::clamp(std::lround(sent * 32767.0), -32767l, 32767l));
  }

 private:
  double x_ = 0, y_ = 0, idle_ = 0;
};

}  // namespace bf1943::controls
