---
lang: en-GB
---

# Keyboard controls and native-style prompts

The keyboard driver distinguishes gameplay, native game menus, the title screen and the PC overlay. Native screens use Enter to confirm, Escape to go back, arrows to navigate and Page Up/Down for shoulder actions. The game supplies explicit `enterMenu` and `enableGameInput` commands; these override a retained soldier or vehicle entity on deployment screens. Keys held across a context change stay suppressed until released.

Gameplay retains editable bindings and controller remapping. Digital keyboard axes keep their directions regardless of controller dead zones or inversion settings. I/K control pitch, J/L control roll and A/D control rudder in an aircraft. W supplies vehicle throttle; S supplies the original left-bumper action, which is brake on land and free look in an aircraft. Mouse toggle aim retains its input source after release, and focus/menu transitions clear latched aim, mouse movement and vehicle targets.

The overlay uses one keyboard router for its pages. Text entry and binding capture own their keys. Ctrl/Alt chords do not also confirm or navigate rows; Ctrl+Page Up/Down remains available for tabs while editing a direct-connect address. Flat paper/ink keycaps and native atlas replacements display actual bindings and controller remaps, including comma-separated alternative keys. The controls page retains its two-column arrangement and shows current keyboard bindings.

Configure with `-DBF_LOCAL_BUILD=ON` to prevent upstream automatic updates or repair from replacing a modified executable. This option defaults to OFF and leaves ordinary upstream builds unchanged. Developer-only pad logging includes both sticks and the current routing context for unattended tests.

Rebuild the SDK with `sdk\src-bf\build_bf.bat` before rebuilding the game: these changes require its updated keyboard driver and exported input-state accessors. Keep the matching rebuilt `rexruntime.dll` alongside the game executable.

## Focused tests

From the repository root in a Visual Studio x64 developer command prompt:

```bat
mkdir input\checks
cl /nologo /std:c++20 /EHsc /W4 /WX /Isdk\src-bf\thirdparty\imgui bf\tests\menu_keyboard_tests.cpp /Feinput\checks\menu.exe /Foinput\checks\menu.obj
input\checks\menu.exe
cl /nologo /std:c++20 /EHsc /W4 /WX /Ibf\src bf\tests\hud_keyboard_prompts_tests.cpp /Feinput\checks\hud.exe /Foinput\checks\hud.obj
input\checks\hud.exe
cl /nologo /std:c++20 /EHsc /W4 /WX /Ibf\src /Isdk\src-bf\include bf\tests\keyboard_controls_tests.cpp /Feinput\checks\controls.exe /Foinput\checks\controls.obj
input\checks\controls.exe
```

The gameplay suite covers 452 checks, including menu commands overriding a stale vehicle state, native accept/back, modifier chords, every context transition, digital axes and aim toggling. The menu and HUD suites cover keyboard ownership, command routing, actual bindings and bounded glyph rasterisation.

For runtime verification, use a hidden window, a synthetic profile and a loopback backend. Scripted keys must pass through the keyboard driver's `SetTestKey` path; injecting an already-mapped controller state alone does not verify keyboard routing. Observe the final pad state, native menu commands and captured game frames together: a retained seated flag alone is insufficient evidence that a vehicle is being controlled. Keep profiles, logs, framebuffer captures, generated game code and game assets local.

## Limits

This work preserves the original aircraft physics. Relative mouse steering and its verification are described in [MOUSE-FLIGHT.md](MOUSE-FLIGHT.md). HUD atlas changes apply when the relevant textures reload; an immediate refresh after live rebinding or input-device switching is not guaranteed. Synthetic background tests do not replace testing physical keyboard events, live focus changes or prolonged multiplayer sessions.

For local sky conversion, see [SKY-CONVERSION.md](../tools/hdpack/SKY-CONVERSION.md). No game files or converted texture pack are included.
