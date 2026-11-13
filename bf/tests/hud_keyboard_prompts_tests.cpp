// Standalone regressions for HUD binding labels and CPU-generated native-style keycaps. No game or SDK required.
#include <cstdio>
#include <map>
#include <string>

#include "hud_keyboard_prompts.h"

static int failures = 0;
#define CHECK(condition) do { if (!(condition)) { std::printf("FAIL line %d: %s\n", __LINE__, #condition); ++failures; } } while (0)

static bool HasInk(const bf1943::hud::KeyPicture& p) {
  for (size_t i = 3; i < p.rgba.size(); i += 4) if (p.rgba[i]) return true;
  return false;
}

int main() {
  using namespace bf1943::hud;
  // The shipped alternative bindings must remain keyboard keys, rather than an unknown image filename.
  CHECK(PrimaryBinding("C,Control") == "C");
  CHECK(PrimaryBinding("Up,V") == "Up");
  CHECK(PrimaryBinding(" , \tControl+G , R ") == "Control+G");
  CHECK(PrimaryBinding(",,\t,").empty());
  CHECK(PrimaryBinding("RMB") == "RMB");
  CHECK(KeyLabel("Numpad9") == "N9");
  CHECK(KeyLabel("NumpadEnter") == "NENT");
  CHECK(KeyLabel("Control") == "CTRL");
  std::map<std::string, std::string> settings = {{"keybind_a", "Space"}, {"keybind_b", "E"},
      {"keybind_x", "R"}, {"keybind_rstick_press", "C,Control"}, {"keybind_dpad_up", "Up,V"}};
  auto get = [&](const std::string& flag) { return settings[flag]; };
  CHECK(BindingFor("a", false, get) == "Space");
  CHECK(BindingFor("rs_click", false, get) == "C");
  CHECK(BindingFor("dpad_up", false, get) == "Up");
  settings["bf_remap_a"] = "x";
  settings["bf_remap_x"] = "a";
  CHECK(BindingFor("a", false, get) == "R");
  CHECK(BindingFor("x", false, get) == "Space");
  CHECK(BindingFor("a", true, get) == "Return");
  CHECK(BindingFor("b", true, get) == "Escape");
  settings["bf_remap_a"] = "none";
  settings["bf_remap_x"] = "none";
  CHECK(BindingFor("a", false, get).empty());
  CHECK(BindingFor("unknown", false, get).empty());
  for (const char* key : {"", "A", "Z", "0", "9", "Space", "Return", "Escape", "Shift", "Control",
                          "Up", "Down", "Left", "Right", "LMB", "RMB", "MMB", "Mouse_Simple", "Pause",
                          "ScrollLock", "PrintScreen", "PageUp", "NumpadPlus", "Control+Shift+G", "Plus",
                          "F12", "Control+Alt+Shift+Return", "a+very+long+invalid+hand+edited+binding"}) {
    const auto picture = Keycap(key);
    CHECK(picture.w == 64 && picture.h == 64);
    CHECK(picture.rgba.size() == size_t(picture.w) * picture.h * 4);
    CHECK(HasInk(picture));
    CHECK(picture.rgba[3] == 0);  // The unmodified atlas outside the keycap is transparent.
  }
  CHECK(Keycap("LMB").rgba != Keycap("RMB").rgba);
  CHECK(Keycap("Up").rgba != Keycap("Down").rgba);
  CHECK(Keycap("Control+G").rgba != Keycap("G").rgba);
  CHECK(Keycap("").rgba != Keycap("A").rgba);
  const std::array<std::string, 4> wasd = {"W", "A", "S", "D"}, arrows = {"Up", "Left", "Down", "Right"};
  const auto all = Movement(wasd), horizontal = Movement(wasd, true, false), vertical = Movement(wasd, false, true);
  CHECK(all.w == 192 && all.h == 128);
  CHECK(horizontal.w == 128 && horizontal.h == 64);
  CHECK(vertical.w == 64 && vertical.h == 128);
  CHECK(all.rgba.size() == size_t(all.w) * all.h * 4);
  CHECK(horizontal.rgba.size() == size_t(horizontal.w) * horizontal.h * 4);
  CHECK(vertical.rgba.size() == size_t(vertical.w) * vertical.h * 4);
  CHECK(Movement(arrows).rgba != all.rgba);  // Rebinding movement changes the prompt rather than leaving WASD.
  // Two-axis and one-axis prompts contain exactly the requested keycaps, with no blank pad margins.
  const auto left = Keycap("A"), up = Keycap("W");
  for (int y = 0; y < 64; ++y) for (int x = 0; x < 64; ++x) for (int c = 0; c < 4; ++c) {
    CHECK(horizontal.rgba[(size_t(y) * 128 + x) * 4 + c] == left.rgba[(size_t(y) * 64 + x) * 4 + c]);
    CHECK(vertical.rgba[(size_t(y) * 64 + x) * 4 + c] == up.rgba[(size_t(y) * 64 + x) * 4 + c]);
  }
  std::printf("HUD keyboard prompt regressions: %s\n", failures ? "FAILED" : "passed");
  return failures ? 1 : 0;
}
