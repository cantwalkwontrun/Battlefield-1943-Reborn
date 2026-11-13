// Standalone control-state regressions; no SDK or game launch required.
// Build: cl /std:c++20 /EHsc /I bf/src /I sdk/src-bf/include bf/tests/keyboard_controls_tests.cpp
#include "keyboard_controls.h"
#include <rex/input/mnk/keyboard_routing.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <iterator>

namespace {
int checks = 0, failures = 0;
void Check(bool value, const char* description) {
  ++checks;
  if (!value) {
    ++failures;
    std::printf("FAIL: %s\n", description);
  }
}
}  // namespace

int main() {
  using bf1943::controls::AimToggle;
  using bf1943::controls::KeyboardAxes;

  // A controller with all four axes inverted must not reverse digital walking,
  // flight pitch or flight roll; unheld keys preserve its analog values.
  constexpr std::array<int, 4> controller{-12000, 15000, -9000, 10000};
  for (int mask = 0; mask < 16; ++mask) {
    const KeyboardAxes keys{int16_t(mask & 1 ? 32767 : 0), int16_t(mask & 2 ? -32767 : 0),
                            int16_t(mask & 4 ? -32767 : 0), int16_t(mask & 8 ? 32767 : 0)};
    const std::array<int, 4> keyboard{keys.left_x, keys.left_y, keys.right_x, keys.right_y};
    for (bool seated : {false, true}) {
      const auto result = keys.Apply(controller, seated);
      for (int axis = 0; axis < 4; ++axis) {
        const int expected = keyboard[axis] ? (axis == 1 && seated ? 0 : keyboard[axis]) : controller[axis];
        Check(result[axis] == expected, "digital controls retain their direction and unheld controller axes");
      }
    }
  }

  // Mouse Toggle with controller Hold: a release must leave the sights raised.
  AimToggle aim;
  Check(aim.Update(true, true, true, false).pressed, "mouse press raises toggle aim");
  Check(aim.Update(true, true, true, false).pressed, "held mouse does not toggle repeatedly");
  Check(aim.Update(false, false, true, false).pressed, "mouse release keeps toggle aim raised");
  Check(!aim.Update(true, true, true, false).pressed, "second mouse press lowers toggle aim");
  Check(!aim.Update(false, false, true, false).pressed, "release keeps toggle aim lowered");

  // The opposite combination must retain the controller's toggle across a release.
  aim.Reset();
  Check(aim.Update(true, false, false, true).pressed, "controller press raises toggle aim");
  Check(aim.Update(false, false, false, true).pressed, "controller release keeps toggle aim raised");
  Check(!aim.Update(true, false, false, true).pressed, "controller second press lowers aim");
  aim.Update(false, false, false, true);
  Check(aim.Update(true, true, false, true).pressed, "mouse hold takes over controller toggle");
  Check(!aim.Update(false, false, false, true).pressed, "mouse hold release lowers aim");

  // Focus loss, menus and vehicle entry reset the same state, so the next press
  // starts from lowered sights. Changing a source's setting to Hold also clears it.
  aim.Reset();
  aim.Update(true, true, true, false);
  aim.Update(false, false, true, false);
  aim.Reset();
  Check(!aim.Update(false, false, true, false).pressed, "focus/menu/seat reset clears latched aim");
  Check(aim.Update(true, true, true, false).pressed, "a fresh press works after reset");
  Check(!aim.Update(false, false, false, false).pressed, "changing Toggle to Hold clears latched aim");
  Check(!aim.Update(false, false, true, false).pressed, "re-enabling Toggle does not resurrect old aim");

  using bf1943::controls::NativeMenuNeedsKeyboard;
  bf1943::controls::NativeScreenState native_screen;
  Check(!native_screen.Active(), "unknown native screen state does not override a live soldier");
  native_screen.Observe("enterMenu", "1");
  Check(native_screen.Active(), "the game's explicit menu entry overrides a stale seated soldier");
  Check(NativeMenuNeedsKeyboard(false, false, native_screen.Active(), true, false),
        "a deployment screen routes to native navigation even with a retained vehicle entity");
  native_screen.Observe("enableGameInput", "0");
  native_screen.Observe("enterMenu", "0");
  Check(native_screen.Active(), "closing the movie waits until gameplay input is enabled");
  native_screen.Observe("enableGameInput", "1");
  Check(!native_screen.Active(), "spawning restores gameplay immediately without a timer");
  native_screen.Observe("enableGameInput", "undefined");
  native_screen.Observe("unrelated", "1");
  Check(!native_screen.Active(), "unrelated or invalid menu commands do not change input routing");
  Check(NativeMenuNeedsKeyboard(false, false, false, false, false), "deployment without a local soldier is native navigation");
  Check(!NativeMenuNeedsKeyboard(false, false, false, true, false), "a soldier in the world retains gameplay in the same match stage");
  Check(NativeMenuNeedsKeyboard(false, false, true, true, false), "explicit native pause/help overrides an existing soldier");
  Check(NativeMenuNeedsKeyboard(false, false, false, true, true), "a native confirmation dialog overrides gameplay");
  Check(!NativeMenuNeedsKeyboard(true, false, true, true, true), "overlay consumes input before native menus");
  Check(!NativeMenuNeedsKeyboard(false, true, true, false, true), "automatic menu driver consumes input before native menus");

  using rex::input::mnk::KeyboardContext;
  using rex::input::mnk::KeyboardContextGate;
  using rex::input::mnk::NativeMenuButtons;
  using rex::ui::VirtualKey;
  bool keys[256] = {};
  auto set = [&](VirtualKey key, bool down) { keys[uint16_t(key)] = down; };
  set(VirtualKey::kReturn, true);
  Check(NativeMenuButtons(keys, KeyboardContext::kNativeMenu) == 0x1000, "Enter confirms a native screen");
  Check(NativeMenuButtons(keys, KeyboardContext::kTitle) == 0x0010, "Enter presses Start on the title screen");
  set(VirtualKey::kReturn, false);
  set(VirtualKey::kEscape, true);
  Check(NativeMenuButtons(keys, KeyboardContext::kNativeMenu) == 0x2000, "Esc cancels a native screen");
  set(VirtualKey::kEscape, false);
  set(VirtualKey::kUp, true); set(VirtualKey::kRight, true);
  set(VirtualKey::kPrior, true); set(VirtualKey::kNext, true);
  Check(NativeMenuButtons(keys, KeyboardContext::kNativeMenu) == 0x0309, "arrows and Page Up/Down map to native menu directions/tabs");
  using rex::input::mnk::NativeMenuActionButtons;
  for (auto modifier : {VirtualKey::kControl, VirtualKey::kMenu}) {
    set(modifier, true);
    set(VirtualKey::kReturn, true);
    Check(NativeMenuButtons(keys, KeyboardContext::kNativeMenu) == 0, "OS/editor chords do not confirm or navigate native menus");
    Check(NativeMenuActionButtons(keys, KeyboardContext::kNativeMenu, 0x1010) == 0,
          "retained gameplay bindings cannot bypass native modifier suppression");
    set(modifier, false);
  }
  std::fill(std::begin(keys), std::end(keys), false);
  Check(NativeMenuActionButtons(keys, KeyboardContext::kNativeMenu, 0x1010) == 0x1000,
        "native action bindings retain A but suppress gameplay Start");
  Check(NativeMenuActionButtons(keys, KeyboardContext::kOverlay, 0xffff) == 0,
        "overlay consumes every emulated keyboard gamepad button");

  // Confirm, jump, exit, fire and movement keys held across every context pair
  // must release before a different action can read them.
  constexpr VirtualKey transition_keys[]{VirtualKey::kReturn, VirtualKey::kEscape, VirtualKey::kSpace,
                                          VirtualKey::kE, VirtualKey::kW, VirtualKey::kLButton};
  constexpr KeyboardContext contexts[]{KeyboardContext::kGameplay, KeyboardContext::kNativeMenu,
                                       KeyboardContext::kTitle, KeyboardContext::kOverlay};
  for (auto before : contexts) {
    for (auto after : contexts) {
      if (before == after) continue;
      for (auto key : transition_keys) {
        KeyboardContextGate gate;
        std::fill(std::begin(keys), std::end(keys), false);
        gate.Filter(before, keys);
        set(key, true);
        gate.Filter(after, keys);
        Check(!keys[uint16_t(key)] && gate.Blocked(uint16_t(key)), "a held key is suppressed when context changes");
        set(key, true);  // the hardware still holds it during the next poll
        gate.Filter(after, keys);
        Check(!keys[uint16_t(key)], "suppression persists while the key remains held");
        set(key, false);
        gate.Filter(after, keys);
        Check(!gate.Blocked(uint16_t(key)), "release clears transition suppression");
        set(key, true);
        gate.Filter(after, keys);
        Check(keys[uint16_t(key)], "the next press activates the new context action");
      }
    }
  }

  std::printf("%d checks, %d failures\n", checks, failures);
  return failures ? 1 : 0;
}
