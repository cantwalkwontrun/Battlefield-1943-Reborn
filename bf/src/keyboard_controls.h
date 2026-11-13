#pragma once

#include <array>
#include <cstdint>
#include <string_view>

namespace bf1943::controls {

// The game's Flash menu explicitly enables/disables gameplay input. Soldier
// state alone is insufficient: a dead character may retain its seated flag.
struct NativeScreenState {
  bool menu = false, game_input = true;

  void Observe(std::string_view command, std::string_view value) {
    if (value != "0" && value != "1") return;
    if (command == "enterMenu") menu = value == "1";
    if (command == "enableGameInput") game_input = value == "1";
  }
  bool Active() const { return menu || !game_input; }
};

// A match's loading/deployment screens share its stage number with gameplay.
// Use the actual local-world state and explicit native screens/dialogs instead.
inline bool NativeMenuNeedsKeyboard(bool overlay, bool driving_menus, bool passing, bool in_world, bool dialog) {
  return !overlay && !driving_menus && (passing || !in_world || dialog);
}

// Controller dead zones and inversion settings must not change a bound digital
// key's direction. An unheld key leaves that controller axis untouched.
struct KeyboardAxes {
  int16_t left_x = 0, left_y = 0, right_x = 0, right_y = 0;

  std::array<int, 4> Apply(std::array<int, 4> controller, bool seated) const {
    if (left_x) controller[0] = left_x;
    if (left_y) controller[1] = seated ? 0 : left_y;  // W/S become throttle/brake.
    if (right_x) controller[2] = right_x;
    if (right_y) controller[3] = right_y;
    return controller;
  }
};

struct AimResult {
  bool pressed = false;
  bool toggle = false;
};

// Keep the source of an aim press until its next edge: releasing a mouse aim
// key must not switch to the controller's Hold setting and clear its toggle.
struct AimToggle {
  bool raised = false, was_down = false, keyboard = false;

  void Reset() { raised = was_down = keyboard = false; }

  AimResult Update(bool down, bool keyboard_down, bool keyboard_toggle, bool controller_toggle) {
    const bool edge = down && !was_down;
    if (edge) keyboard = keyboard_down;
    const bool toggle = keyboard ? keyboard_toggle : controller_toggle;
    if (!toggle) raised = false;
    else if (edge) raised = !raised;
    was_down = down;
    return {toggle ? raised : down, toggle};
  }
};

}  // namespace bf1943::controls
