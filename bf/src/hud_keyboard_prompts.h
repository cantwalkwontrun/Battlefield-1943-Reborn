// Native HUD keycaps: fixed atlas rectangles, actual bindings, no controller-icon fallback.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bf1943::hud {

inline std::string PrimaryBinding(std::string_view bindings) {
  while (!bindings.empty()) {
    const size_t comma = bindings.find(',');
    std::string_view key = bindings.substr(0, comma);
    const size_t first = key.find_first_not_of(" \t\r\n");
    if (first != std::string_view::npos) {
      key = key.substr(first, key.find_last_not_of(" \t\r\n") - first + 1);
      return std::string(key);
    }
    if (comma == std::string_view::npos) break;
    bindings.remove_prefix(comma + 1);
  }
  return {};
}

template <typename GetFlag>
std::string BindingFor(std::string_view button, bool menu, GetFlag get) {
  if (menu && button == "a") return "Return";
  if (menu && button == "b") return "Escape";
  struct Binding { std::string_view button, key_flag, remap; };
  static constexpr Binding bindings[] = {
      {"a", "keybind_a", "a"}, {"b", "keybind_b", "b"}, {"x", "keybind_x", "x"}, {"y", "keybind_y", "y"},
      {"lb", "keybind_left_shoulder", "lb"}, {"rb", "keybind_right_shoulder", "rb"},
      {"lt", "keybind_left_trigger", "lt"}, {"rt", "keybind_right_trigger", "rt"},
      {"start", "keybind_start", "start"}, {"back", "keybind_back", "back"},
      {"dpad_up", "keybind_dpad_up", "up"}, {"dpad_down", "keybind_dpad_down", "down"},
      {"dpad_left", "keybind_dpad_left", "left"}, {"dpad_right", "keybind_dpad_right", "right"},
      {"ls_click", "keybind_lstick_press", "ls"}, {"rs_click", "keybind_rstick_press", "rs"}};
  std::string_view target;
  for (const auto& b : bindings) if (b.button == button) target = b.remap;
  if (target.empty()) return {};
  for (const auto& b : bindings) {
    std::string mapped = get("bf_remap_" + std::string(b.remap));
    if (mapped.empty()) mapped = b.remap;
    if (mapped != target) continue;
    const std::string key = PrimaryBinding(get(std::string(b.key_flag)));
    if (!key.empty()) return key;
  }
  return {};  // Unbound is explicit; never show a controller fallback for a keyboard action.
}

inline std::string KeyLabel(std::string_view key) {
  struct Name { std::string_view key, label; };
  static constexpr Name names[] = {
      {"Return", "ENT"}, {"Enter", "ENT"}, {"Escape", "ESC"}, {"Space", "SPC"},
      {"Control", "CTRL"}, {"Shift", "SHFT"}, {"Backspace", "BKSP"},
      {"Delete", "DEL"}, {"Insert", "INS"}, {"PageUp", "PGUP"}, {"PageDown", "PGDN"},
      {"PrintScreen", "PRNT"}, {"ScrollLock", "SCRL"}, {"CapsLock", "CAPS"}, {"NumLock", "NUM"},
      {"NumpadEnter", "NENT"}, {"NumpadPlus", "N+"}, {"NumpadMinus", "N-"},
      {"NumpadStar", "N*"}, {"NumpadSlash", "N/"}, {"LBracket", "["}, {"RBracket", "]"},
      {"Semicolon", ";"}, {"Quote", "'"}, {"Backtick", "`"}, {"Minus", "-"},
      {"Plus", "+"}, {"Comma", ","}, {"Period", "."}, {"Slash", "/"}, {"Backslash", "\\"}};
  for (const auto& name : names) if (key == name.key) return std::string(name.label);
  if (key.size() == 7 && key.substr(0, 6) == "Numpad") return "N" + std::string(key.substr(6));
  std::string out(key);
  for (char& c : out) if (c >= 'a' && c <= 'z') c -= 'a' - 'A';
  return out.empty() ? "--" : out;
}

struct KeyPicture {
  int w = 64, h = 64;
  std::vector<uint8_t> rgba = std::vector<uint8_t>(64 * 64 * 4, 0);
};

// A small explicit bitmap alphabet keeps renderer-thread generation independent of ImGui's font atlas/GPU.
inline std::array<uint8_t, 7> Letter(char c) {
  switch (c) {
    case 'A': return {14,17,17,31,17,17,17}; case 'B': return {30,17,17,30,17,17,30};
    case 'C': return {14,17,16,16,16,17,14}; case 'D': return {30,17,17,17,17,17,30};
    case 'E': return {31,16,16,30,16,16,31}; case 'F': return {31,16,16,30,16,16,16};
    case 'G': return {14,17,16,23,17,17,15}; case 'H': return {17,17,17,31,17,17,17};
    case 'I': return {14,4,4,4,4,4,14}; case 'J': return {7,2,2,2,18,18,12};
    case 'K': return {17,18,20,24,20,18,17}; case 'L': return {16,16,16,16,16,16,31};
    case 'M': return {17,27,21,21,17,17,17}; case 'N': return {17,25,21,19,17,17,17};
    case 'O': return {14,17,17,17,17,17,14}; case 'P': return {30,17,17,30,16,16,16};
    case 'Q': return {14,17,17,17,21,18,13}; case 'R': return {30,17,17,30,20,18,17};
    case 'S': return {15,16,16,14,1,1,30}; case 'T': return {31,4,4,4,4,4,4};
    case 'U': return {17,17,17,17,17,17,14}; case 'V': return {17,17,17,17,17,10,4};
    case 'W': return {17,17,17,21,21,21,10}; case 'X': return {17,17,10,4,10,17,17};
    case 'Y': return {17,17,10,4,4,4,4}; case 'Z': return {31,1,2,4,8,16,31};
    case '0': return {14,17,19,21,25,17,14}; case '1': return {4,12,4,4,4,4,14};
    case '2': return {14,17,1,2,4,8,31}; case '3': return {30,1,1,14,1,1,30};
    case '4': return {2,6,10,18,31,2,2}; case '5': return {31,16,16,30,1,1,30};
    case '6': return {14,16,16,30,17,17,14}; case '7': return {31,1,2,4,8,8,8};
    case '8': return {14,17,17,14,17,17,14}; case '9': return {14,17,17,15,1,1,14};
    case '+': return {0,4,4,31,4,4,0}; case '-': return {0,0,0,31,0,0,0};
    case '*': return {0,21,14,31,14,21,0}; case '/': return {1,2,2,4,8,8,16};
    case '\\': return {16,8,8,4,2,2,1}; case '[': return {14,8,8,8,8,8,14};
    case ']': return {14,2,2,2,2,2,14}; case ';': return {0,4,4,0,4,4,8};
    case '\'': return {4,4,8,0,0,0,0}; case '`': return {8,4,0,0,0,0,0};
    case ',': return {0,0,0,0,4,4,8}; case '.': return {0,0,0,0,0,4,4};
    case ' ': return {}; default: return {14,17,1,2,4,0,4};
  }
}

inline void Fill(KeyPicture& p, int x0, int y0, int x1, int y1, std::array<uint8_t, 4> colour) {
  for (int y = std::max(0, y0); y < std::min(p.h, y1); ++y)
    for (int x = std::max(0, x0); x < std::min(p.w, x1); ++x)
      std::copy(colour.begin(), colour.end(), p.rgba.begin() + (size_t(y) * p.w + x) * 4);
}

inline void Text(KeyPicture& p, std::string_view text, int y, int max_scale = 5) {
  if (text.empty()) return;
  const int scale = std::max(1, std::min(max_scale, 52 / int(text.size() * 6 - 1)));
  y += (max_scale - scale) * 7 / 2;
  int x = (p.w - int(text.size() * 6 - 1) * scale) / 2;
  for (char c : text) {
    const auto glyph = Letter(c);
    for (int row = 0; row < 7; ++row)
      for (int col = 0; col < 5; ++col)
        if (glyph[row] & (1 << (4 - col)))
          Fill(p, x + col * scale, y + row * scale, x + (col + 1) * scale, y + (row + 1) * scale,
               {235, 233, 216, 255});
    x += 6 * scale;
  }
}

inline KeyPicture Keycap(std::string_view binding) {
  KeyPicture p;
  // Flat paper/ink colours and square edges echo BF1943's HUD rather than a modern 3D keyboard theme.
  Fill(p, 3, 5, 62, 63, {0, 0, 0, 150});
  Fill(p, 1, 1, 61, 59, {222, 220, 201, 255});
  Fill(p, 4, 4, 58, 56, {22, 25, 22, 235});
  if (binding == "LMB" || binding == "RMB" || binding == "MMB" || binding == "Mouse_Simple") {
    Fill(p, 18, 10, 44, 49, {222, 220, 201, 255});
    Fill(p, 21, 13, 41, 46, {22, 25, 22, 255});
    Fill(p, 30, 12, 33, 28, {222, 220, 201, 255});
    Fill(p, 19, 27, 43, 30, {222, 220, 201, 255});
    if (binding == "LMB") Fill(p, 21, 13, 30, 27, {222, 220, 201, 255});
    if (binding == "RMB") Fill(p, 33, 13, 41, 27, {222, 220, 201, 255});
    if (binding == "MMB") Fill(p, 29, 15, 34, 25, {222, 220, 201, 255});
    return p;
  }
  if (binding == "Up" || binding == "Down" || binding == "Left" || binding == "Right") {
    const int dx = binding == "Left" ? -1 : binding == "Right" ? 1 : 0;
    const int dy = binding == "Up" ? -1 : binding == "Down" ? 1 : 0;
    for (int step = -12; step <= 12; ++step)
      Fill(p, 31 + dx * step - 2, 29 + dy * step - 2, 31 + dx * step + 3, 29 + dy * step + 3,
           {235, 233, 216, 255});
    for (int step = 0; step <= 10; ++step)
      for (int side : {-1, 1}) {
        const int x = 31 + dx * (12 - step) - dy * step * side;
        const int y = 29 + dy * (12 - step) + dx * step * side;
        Fill(p, x - 2, y - 2, x + 3, y + 3, {235, 233, 216, 255});
      }
    return p;
  }
  std::vector<std::string> lines;
  while (!binding.empty()) {
    const size_t plus = binding.find('+');
    lines.push_back(KeyLabel(binding.substr(0, plus)));
    if (plus == std::string_view::npos) break;
    binding.remove_prefix(plus + 1);
  }
  if (lines.empty()) lines.push_back("--");
  // Chords retain every modifier, with a bounded raster even for invalid hand-edited settings.
  if (lines.size() > 4) lines = {"?"};
  const int max_scale = std::max(1, std::min(5, 44 / int(lines.size() * 9 - 2)));
  const int height = int(lines.size()) * 9 * max_scale - 2 * max_scale;
  int y = (59 - height) / 2;
  for (const auto& line : lines) {
    if (line.size() > 8) Text(p, "?", y, max_scale); else Text(p, line, y, max_scale);
    y += 9 * max_scale;
  }
  return p;
}

// The four bound movement keys in their usual spatial arrangement; axis hints show only their own pair.
inline KeyPicture Movement(const std::array<std::string, 4>& keys, bool horizontal = true, bool vertical = true) {
  KeyPicture p;
  p.w = horizontal ? (vertical ? 192 : 128) : 64, p.h = vertical ? 128 : 64;
  p.rgba.assign(size_t(p.w) * p.h * 4, 0);
  const int xs[] = {64, 0, 64, 128}, ys[] = {0, 64, 64, 64};  // up, left, down, right
  for (size_t i = 0; i < keys.size(); ++i) {
    if ((i == 0 || i == 2) ? !vertical : !horizontal) continue;
    const auto cap = Keycap(keys[i]);
    for (int y = 0; y < cap.h; ++y)
      for (int x = 0; x < cap.w; ++x) {
        const int to_y = vertical ? ys[i] + y : y;
        const int to_x = !horizontal ? x : !vertical ? (i == 1 ? 0 : 64) + x : xs[i] + x;
        std::copy_n(cap.rgba.data() + (size_t(y) * cap.w + x) * 4, 4,
                    p.rgba.data() + (size_t(to_y) * p.w + to_x) * 4);
      }
  }
  return p;
}

}  // namespace bf1943::hud
