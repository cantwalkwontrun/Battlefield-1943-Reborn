// Local keyboard routing regression checks; no window or game is started.
#include "../src/menu_keyboard.h"

#include <cassert>
#include <initializer_list>
#include <iostream>

using namespace bf1943;

static MenuInput Read(MenuKeyboardContext context, std::initializer_list<ImGuiKey> keys, bool repeat_only = false) {
  return ReadMenuKeyboard(context, [&](ImGuiKey key, bool repeat) {
    if (repeat_only && !repeat) return false;
    for (ImGuiKey pressed : keys) if (pressed == key) return true;
    return false;
  });
}

int main() {
  const auto commands = Read({}, {ImGuiKey_F, ImGuiKey_R, ImGuiKey_P, ImGuiKey_Tab});
  assert(commands.x && commands.y && commands.start && commands.view);
  assert(!commands.accept && !commands.prev_tab && !commands.next_tab);
  for (ImGuiKey key : {ImGuiKey_Enter, ImGuiKey_KeypadEnter}) assert(Read({}, {key}).accept);
  assert(Read({}, {ImGuiKey_Escape}).back);
  assert(Read({}, {ImGuiKey_Q}).prev_tab && Read({}, {ImGuiKey_E}).next_tab);

  MenuKeyboardContext tabs;
  tabs.tab_navigation = true;
  assert(Read(tabs, {ImGuiKey_Tab}).next_tab && !Read(tabs, {ImGuiKey_Tab}).view);
  tabs.shift = true;
  assert(Read(tabs, {ImGuiKey_Tab}).prev_tab && !Read(tabs, {ImGuiKey_Tab}).next_tab);

  MenuKeyboardContext text;
  text.text_entry = true;
  const auto edited = Read(text, {ImGuiKey_E, ImGuiKey_Q, ImGuiKey_F, ImGuiKey_R, ImGuiKey_P, ImGuiKey_Tab});
  assert(!edited.prev_tab && !edited.next_tab && !edited.x && !edited.y && !edited.start && !edited.view);
  assert(Read(text, {ImGuiKey_Enter}).accept && Read(text, {ImGuiKey_Escape}).back);
  text.control = true;
  assert(Read(text, {ImGuiKey_PageUp}).prev_tab && Read(text, {ImGuiKey_PageDown}).next_tab);
  assert(!Read(text, {ImGuiKey_V, ImGuiKey_R}).y);
  text.alt = true;
  assert(!Read(text, {ImGuiKey_PageDown}).next_tab);
  assert(!Read(text, {ImGuiKey_Enter, ImGuiKey_UpArrow}).accept);
  assert(!Read(text, {ImGuiKey_Enter, ImGuiKey_UpArrow}).up);
  MenuKeyboardContext alt;
  alt.alt = true;
  assert(!Read(alt, {ImGuiKey_Enter}).accept);

  MenuKeyboardContext capture;
  capture.binding_capture = true;
  const auto captured = Read(capture, {ImGuiKey_Escape, ImGuiKey_Enter, ImGuiKey_UpArrow, ImGuiKey_Tab});
  assert(!captured.back && !captured.accept && !captured.up && !captured.next_tab && !captured.view);

  const auto held = Read({}, {ImGuiKey_DownArrow, ImGuiKey_Enter, ImGuiKey_Escape, ImGuiKey_E}, true);
  assert(held.down && !held.accept && !held.back && !held.next_tab);
  const auto combined = CombineMenuInputs(commands, Read({}, {ImGuiKey_Enter, ImGuiKey_UpArrow}));
  assert(combined.up && combined.accept && combined.x && combined.y && combined.start && combined.view);
  std::cout << "Menu keyboard routing checks passed.\n";
}
