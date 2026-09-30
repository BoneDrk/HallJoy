# Curve reset button — 2026-09-29

Owner request: a reset button next to the curve settings that restores a
straight, even curve without changing deadzones.

## Behaviour

- Location: Configuration page, right end of the `Mode` / `Preset` row,
  right-aligned with the graph edge; 28x28 px (DPI-scaled). The Preset combo is
  narrowed by the button width plus an 8 px gap.
- Style: same face as the PremiumCombo boxes in the row (ControlBg fill, thin
  Border, rounded corners); hover brightens the border and turns the ↺ icon
  accent-coloured. Disabled (muted icon, arrow cursor) when the curve is already
  straight.
- Action: control points move onto the Start→End line at 1/3 and 2/3. Low/high
  deadzones, anti-deadzone, output cap, invert and Smooth/Linear mode are kept.
  Weights become the mode-neutral values (Smooth 0.5, Linear 1.0). Collinear
  control points give an exact straight line in both modes (the rational Bezier
  of collinear points stays on that line).
- Target: the same as a graph edit — the selected key when its override is on,
  otherwise the global curve. Undo history, curve morph animation, preset dirty
  (Save) icon and debounced settings save follow the existing edit path.
- Mouse only; no keyboard handling (main-window keyboard policy).

Files: `keyboard_keysettings_panel.cpp` (rect, draw, hit-test, action),
`keyboard_subpages.cpp` (WM_MOUSELEAVE forwarding for hover).
Visual evaluation is the owner's; the agent verified build and automated checks only.

Build: `tools/build_release.ps1` EXIT=0, installed `build/bin/Release/x64/HallJoy.exe` SHA256 `35ddf5f0f6cda3794ed21c4e67d579192b610d1077f59e01ebe350b9458296eb`.

## Revision 2 (owner feedback, same day)

- The ↺ icon was unclear. The button now shows a miniature straight graph: an
  orange diagonal from bottom-left to top-right with the filled area below it,
  using the graph's exact colour (255,170,0) and its vertical gradient (alpha
  200 → 25). Hover strengthens the line; disabled state is faint.
- Preset combo: while a saved preset has unsaved edits, a Revert icon appears
  to the left of the Save icon (`PremiumCombo::ExtraIconKind::SaveAndRevert`).
  Revert reloads the preset from disk through the preset-selection path (undo
  history, morph, clean state). The retained Configuration face now hit-tests
  both icons, so Save/Revert clicks no longer open the dropdown instead.
