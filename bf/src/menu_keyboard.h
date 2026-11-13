// Keyboard routing shared by the launcher and the Battlefield-style menus.
#pragma once

#include <imgui.h>

namespace bf1943 {

struct MenuInput {
  bool up = false, down = false, left = false, right = false, accept = false, back = false;
  bool prev_tab = false, next_tab = false;
  bool x = false, y = false, start = false, view = false;
};

struct MenuKeyboardContext {
  bool text_entry = false;
  bool binding_capture = false;
  bool tab_navigation = false;
  bool shift = false;
  bool control = false;
  bool alt = false;
};

constexpr MenuInput CombineMenuInputs(const MenuInput& a, const MenuInput& b) {
  return {a.up || b.up, a.down || b.down, a.left || b.left, a.right || b.right,
          a.accept || b.accept, a.back || b.back, a.prev_tab || b.prev_tab, a.next_tab || b.next_tab,
          a.x || b.x, a.y || b.y, a.start || b.start, a.view || b.view};
}

// A binding capture owns every key. Text entry keeps navigation/confirmation,
// but letters and plain Tab must remain available to the editor.
template <class Pressed>
MenuInput ReadMenuKeyboard(const MenuKeyboardContext& context, Pressed pressed) {
  MenuInput in;
  if (context.binding_capture) return in;
  if (context.control && !context.alt) {
    in.prev_tab = pressed(ImGuiKey_PageUp, false);
    in.next_tab = pressed(ImGuiKey_PageDown, false);
  }
  // OS/editor chords (Alt+Enter, Ctrl+V, Ctrl+arrows) must not also select
  // a row, change a value or confirm a dialog.
  if (context.control || context.alt) return in;
  in.up = pressed(ImGuiKey_UpArrow, true);
  in.down = pressed(ImGuiKey_DownArrow, true);
  in.left = pressed(ImGuiKey_LeftArrow, true);
  in.right = pressed(ImGuiKey_RightArrow, true);
  in.accept = pressed(ImGuiKey_Enter, false) || pressed(ImGuiKey_KeypadEnter, false);
  in.back = pressed(ImGuiKey_Escape, false);
  if (context.text_entry) return in;
  in.prev_tab |= pressed(ImGuiKey_Q, false);
  in.next_tab |= pressed(ImGuiKey_E, false);
  in.x = pressed(ImGuiKey_F, false);
  in.y = pressed(ImGuiKey_R, false);
  in.start = pressed(ImGuiKey_P, false);
  if (pressed(ImGuiKey_Tab, false)) {
    if (context.tab_navigation) {
      in.prev_tab |= context.shift;
      in.next_tab |= !context.shift;
    } else {
      in.view = true;
    }
  }
  return in;
}

}  // namespace bf1943
