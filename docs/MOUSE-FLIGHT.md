---
lang: en-GB
---

# Relative mouse steering for aircraft

The mouse supplies aircraft pitch and roll. Vehicle throttle, rudder, weapons and seat controls retain their editable bindings. This is mouse steering without a controller; it does not add automatic throttle, auto-levelling or an autopilot.

Raw motion is accumulated once per displayed frame, converted from counts to counts per second and smoothed over 25 ms. Encoding compensates for the pad mapping's 15% dead zone and the subsequent vehicle concept's 25% dead zone once each, so gentle movements reach the control surfaces. Stopping the mouse returns the command to exact neutral within 100 ms. A frame stall, focus loss, native menu, seat change or keyboard/controller steering clears the previous command. Vehicle mouse sensitivity and mouse vertical inversion still apply.

Two input conflicts are removed:

- The SDK exposes `mnk_mouse_stick`, enabled by default for other users of the SDK. Reborn disables it because its own raw mouse paths already supply aiming and steering. This leaves mouse capture independent of stick emulation.
- Aircraft elevators and ailerons also use the game's `ChildRotationBodyData` helper. Its reflected `ResetForceModifier` at offset `0x40` identifies a restoring force towards neutral. These parts bypass the turret target-angle controller. Parts without that restoring force retain mouse turret aiming. The original aircraft physics and server input remain unchanged.

Rebuild both the SDK and the game. Deploy the matching `rexruntime.dll` and executable together. Use `BF_LOCAL_BUILD=ON` for a modified local installation, and keep developer tools disabled in the installed build.

## Reproducible checks

From a Visual Studio x64 developer command prompt, run the pure input tests:

```bat
cl /nologo /std:c++20 /EHsc /W4 /WX /Ibf\src bf\tests\mouse_flight_tests.cpp /Feinput\checks\flight.exe /Foinput\checks\flight.obj
input\checks\flight.exe
```

Create `input\checks` first if necessary. The tests cover steady motion at 30, 60, 120, 144 and 240 FPS, repeated pad reads, neutral return, inversion, bounded sweeps, invalid timing and reset behaviour.

`tools/test_mouse_flight.ps1` runs a developer client, a dedicated server and the Python backend with a synthetic profile and loopback addresses. Supply `ClientExe`, `ServerExe`, `GameDir`, `CacheDir` and a new `OutputDirectory`. Both executables require their matching runtime files. The cache directory must already exist. Select `FrameRate` 30, 60 or 120 and `Scenario` `flight`, `inverted`, `handoff` or `turret`. `PackFile` optionally enables a locally converted HD pack in the synthetic profile.

The client and server start hidden and below normal priority. The Coral Sea fixture follows the parked Corsair's fixed map position, then requires a matching 57-part tree within 15 metres of the seated player before scripting starts. An unavailable requested target never falls back to an unguided walk. Scripted motion enters the same frame accumulators as raw window motion; scripted keys use the keyboard driver's test path. No operating-system pointer or keyboard events are sent. The harness refuses to run alongside another game or occupied test listener ports and stops only its own child processes at the deadline.

Run `python tools/analyse_mouse_flight.py <output-directory>` afterwards. Aircraft checks require the targeted 57-part Corsair, calibrated positive/negative commands, neutral return, altitude gain, travel, sustained gameplay, the requested sampling rate and complete framebuffer readbacks. Stronger sweeps additionally inspect the original control-surface inputs and angles. The analyser requires Pillow to decode the captured frames. Keep profiles, game files, caches, logs, captured frames and converted packs local.

## Recorded background verification

Windows developer-client runs with a loopback server and synthetic profile passed the following checks. No operating-system input was sent. The original vehicle physics remained active, and the local HD sky pack was loaded during the runs.

| Scenario | Passed checks | Aircraft/vehicle gameplay observed |
| --- | ---: | ---: |
| Relative mouse, 30 FPS | 47/47 | 59.4 s |
| Relative mouse, 60 FPS | 47/47 | 59.6 s |
| Relative mouse, 120 FPS | 48/48 | 59.6 s |
| Vertical inversion, 60 FPS | 48/48 | 59.8 s |
| Keyboard handover, 60 FPS | 26/26 | 51.7 s |
| Sherman turret yaw and gun elevation, 30 FPS | 19/19 | 52.3 s |

The extra check in the later flight runs confirms the target tree before scripting. Strong mouse sweeps produced opposite bank and pitch changes in the rendered aircraft view; gentle commands reached the original control-surface input mapping. The focused suites also passed 49 mouse-flight checks, 452 keyboard checks, menu routing and HUD prompt regressions. These results describe synthetic coverage, not a physical-device or extended multiplayer playtest. Raw evidence and game-derived files are retained locally and are not included in the repository.

## Verification limits

Synthetic runs exercise the real game and renderer, but bypass physical device events and window focus. They do not establish how every mouse, DPI setting, aircraft type, server or extended multiplayer session feels. Physical mouse handling remains a separate playtest.
