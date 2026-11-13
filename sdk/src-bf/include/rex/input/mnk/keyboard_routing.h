#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

#include <rex/ui/virtual_key.h>

namespace rex::input::mnk {

enum class KeyboardContext : uint8_t { kGameplay, kNativeMenu, kTitle, kOverlay };

// A key that confirmed a deployment/menu selection cannot become jump, exit or
// pause when that screen closes. It must be released before its new action.
class KeyboardContextGate {
 public:
  void Filter(KeyboardContext context, bool (&keys)[256]) {
    if (initialised_ && context != context_) {
      for (std::size_t i = 0; i < blocked_.size(); ++i) blocked_[i] = blocked_[i] || keys[i];
    }
    initialised_ = true;
    context_ = context;
    for (std::size_t i = 0; i < blocked_.size(); ++i) {
      blocked_[i] = blocked_[i] && keys[i];
      keys[i] = keys[i] && !blocked_[i];
    }
  }

  void Reset() { blocked_.fill(false); initialised_ = false; }
  bool Blocked(uint16_t key) const { return key < blocked_.size() && blocked_[key]; }

 private:
  std::array<bool, 256> blocked_{};
  KeyboardContext context_ = KeyboardContext::kGameplay;
  bool initialised_ = false;
};

inline bool NativeMenuModifiersHeld(const bool (&keys)[256]) {
  return keys[static_cast<uint16_t>(rex::ui::VirtualKey::kControl)] ||
         keys[static_cast<uint16_t>(rex::ui::VirtualKey::kMenu)];
}

inline uint16_t NativeMenuButtons(const bool (&keys)[256], KeyboardContext context) {
  if (NativeMenuModifiersHeld(keys)) return 0;
  using rex::ui::VirtualKey;
  auto held = [&](VirtualKey key) { return keys[static_cast<uint16_t>(key)]; };
  uint16_t buttons = 0;
  if (held(VirtualKey::kReturn)) buttons |= context == KeyboardContext::kTitle ? 0x0010 : 0x1000;
  if (held(VirtualKey::kEscape)) buttons |= 0x2000;
  if (held(VirtualKey::kUp)) buttons |= 0x0001;
  if (held(VirtualKey::kDown)) buttons |= 0x0002;
  if (held(VirtualKey::kLeft)) buttons |= 0x0004;
  if (held(VirtualKey::kRight)) buttons |= 0x0008;
  if (held(VirtualKey::kPrior)) buttons |= 0x0100;
  if (held(VirtualKey::kNext)) buttons |= 0x0200;
  return buttons;
}

inline uint16_t NativeMenuActionButtons(const bool (&keys)[256], KeyboardContext context, uint16_t bound_buttons) {
  if (context == KeyboardContext::kOverlay || NativeMenuModifiersHeld(keys)) return 0;
  // The keyboard's Start binding must not also pause when Enter confirms.
  return uint16_t(bound_buttons & ~uint16_t(0x0010)) | NativeMenuButtons(keys, context);
}

}  // namespace rex::input::mnk
