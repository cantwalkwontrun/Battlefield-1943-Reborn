// Language: en-GB
#include "mouse_flight.h"

#include <cmath>
#include <cstdio>
#include <limits>

static int checks = 0, failures = 0;
static void Check(bool condition, const char* description) {
  ++checks;
  if (!condition) { ++failures; std::printf("FAIL: %s\n", description); }
}

int main() {
  using bf1943::controls::MouseFlight;
  double reference = 0;
  for (int fps : {30, 60, 120, 144, 240}) {
    MouseFlight flight;
    const double dt = 1.0 / fps;
    for (int frame = 0; frame < fps * 2; ++frame) flight.Update(250 * dt, -125 * dt, dt, 1);
    Check(std::fabs(flight.X() - 0.25) < 0.00001, "constant mouse rate has the same steady roll at every frame rate");
    Check(std::fabs(flight.Y() - 0.125) < 0.00001, "mouse up produces climb input at every frame rate");
    Check(flight.Y(true) == -flight.Y(), "vertical inversion changes only the pitch sign");
    if (reference) Check(std::abs(int(MouseFlight::GameAxis(flight.X())) - int(reference)) <= 1,
                         "guest axis encoding is independent of frame rate");
    reference = MouseFlight::GameAxis(flight.X());
    const int repeated = MouseFlight::GameAxis(flight.X());
    int accumulated = 0;
    for (int read = 0; read < 100; ++read) accumulated += MouseFlight::GameAxis(flight.X());
    Check(accumulated == 100 * repeated, "repeated gamepad reads do not consume or amplify motion");
    for (int frame = 0; frame < fps / 5 + 1; ++frame) flight.Update(0, 0, dt, 1);
    Check(flight.X() == 0 && flight.Y() == 0, "stopped mouse returns exactly to neutral");
    Check(MouseFlight::GameAxis(flight.X()) == 0, "neutral does not leave a dead-zone-sized stick push");
  }
  MouseFlight flight;
  flight.Update(1000000, -1000000, 1.0 / 30, 10);
  Check(flight.X() > 0 && flight.X() <= 1 && flight.Y() > 0 && flight.Y() <= 1, "large sweeps are bounded");
  flight.Update(0, 0, 1, 1);
  Check(flight.X() == 0 && flight.Y() == 0, "a long stall discards old motion");
  for (double invalid : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN(), std::numeric_limits<double>::infinity()}) {
    flight.Update(20, 20, invalid, 1);
    Check(flight.X() == 0 && flight.Y() == 0, "invalid timing resets the controller");
  }
  flight.Update(20, 20, 1.0 / 60, 1);
  flight.Reset();
  Check(flight.X() == 0 && flight.Y() == 0, "seat/menu/focus reset clears both axes");
  Check(MouseFlight::GameAxis(1) == 32767 && MouseFlight::GameAxis(-1) == -32767, "full deflections encode symmetrically");
  Check(MouseFlight::GameAxis(0) == 0 && MouseFlight::GameAxis(0.00001) == 0, "noise and idle encode as zero");
  for (double requested : {-0.65, -0.08, 0.02, 0.08, 0.5, 0.65}) {
    const double encoded = std::abs(int(MouseFlight::GameAxis(requested))) / 32767.0;
    const double pad = std::max(0.0, (encoded - 0.15) / 0.85);
    const double mapped = std::max(0.0, (pad - 0.25) / 0.75);
    Check(std::abs(mapped - std::abs(requested)) < 0.00003, "gentle and fast sweeps survive both guest dead zones");
  }
  std::printf("%d mouse flight checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
