# Extended vendor keys: frozen animation and bind capture (2026-10-01)

Owner report: on Keychron the RGB key is drawn oddly. It updates "its own way",
crookedly, and sticks.

## Cause

The Keychron RGB key uses the extended Soup/UAP code `0x404`, the same as
Wooting "Profile 2". HallJoy keeps such codes intact rather than aliasing them
to ordinary HID bytes.

1. `keyboard_render.cpp`, per-tick animation loop. It skipped every
   non-standard code except OEM1 `0x403`, Fn `0x409`, Wooting split keys
   `0x480..0x483` and O3C. For `0x404` (RGB, Profile 2), `0x405` (Profile 3),
   `0x408` (Mode) and other vendor codes in a layout, the press flash
   (impact), selection fade and gear animation were never renewed. Their last
   frame stayed until an unrelated repaint, which looks like sticking or
   uneven updating. Values and dirty bits were correct; only the animation
   driver ignored these keys.
2. `backend.cpp`, bind capture outside the K4 onboard path. It considered
   HIDs 1..255, `0x403`, `0x409`, Wooting split and O3C only, so pressing RGB
   or Wooting Profile/Mode could not be captured for a binding.

Affected beyond Keychron: Wooting Profile 1–3 and Mode keys, and any future
layout with another extended code.

## Fix

- The animation loop no longer filters by code; every drawable code is
  handled. The checks are atomic loads, and the loop already visited every
  code for digital state.
- Bind capture reads every **non-standard key of the current layout**, so
  unused extended codes cost no provider reads.
- `wooting_physical_keys_static_audit.py` now requires both, and fails on the
  previous allow lists.
- Backups: `.local/backups/keyboard_render-before-extended-anim-2026-10-01.cpp`,
  `backend-before-extended-capture-2026-10-01.cpp`.

No visual run by the agent; the owner checks the RGB key.
