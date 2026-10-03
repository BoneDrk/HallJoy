# Keyboard view: Direct2D canvas (2026-10-01)

## Why

With every key pressed at once the keyboard preview dropped frames. Each key
was its own owner-draw window painted with GDI + GDI+ (new `Graphics` object
per element, per key, per frame): ~100–200 µs per key, 30–40 ms per frame for a
full board. The owner asked for a complete rewrite instead of incremental
caching (the retained per-key bitmap cache was abandoned and deleted).

## Architecture

- `keyboard_canvas.h/.cpp` — one child window (`HallJoyKeyboardCanvas`) with an
  `ID2D1HwndRenderTarget` covering the keyboard and the mouse panel.
  - `WM_NCHITTEST → HTTRANSPARENT`: mouse input goes to the key windows below.
  - Frames are locked to the compositor's vblank grid
    (`DwmGetCompositionTimingInfo`: `qpcVBlank`, `qpcRefreshPeriod`, re-read
    every 250 ms). A request renders at once when the current refresh interval
    has no frame yet; otherwise a pacing thread waits on a high-resolution
    waitable timer until 1/16 period after the next vblank and posts one frame.
    Any number of requests within one interval produce one frame.
    - Not `DwmFlush`: it blocks while nothing is composed, so the canvas itself
      starved.
    - Not "previous frame + period": every frame started slightly late, so the
      rate settled below the refresh rate (175 fps at 199 Hz, ~12 % of
      refreshes without a new frame, seen as stutter).
  - While displayed analog values change, frames continue at display rate.
  - `D2DERR_RECREATE_TARGET` and `WM_DISPLAYCHANGE` recreate the target.
  - Z-order: canvas directly above the key/mouse buttons, below every other
    sibling (sub-tabs, support banner, pause card).
- Key and mouse buttons stay as input-only windows (hit testing, capture,
  drag, cursor, gear click). Their `WM_PAINT` validates and requests a canvas
  frame; moves, show/hide, enable and text changes also request a frame. All
  existing `InvalidateRect(key)` call sites therefore keep working unchanged.
  `KeyboardUI_OnAnalogPreview` additionally requests one frame per tick when
  any backend dirty bit was consumed.
- `keyboard_render.cpp` — `KeyboardRender_DrawKeyD2D` draws one key on any
  Direct2D target: background, border, selection glow (also along compound
  contours), inner area, analog fill, impact flash, bound icons or label,
  digital (Windows key-down) dot, override gear, drop-target outline.
  - One anti-aliasing mode (per-primitive) for the whole key; mode switches
    split Direct2D batches. Only the drop-target outline switches to aliased.
  - Every rectangle is pixel-aligned (the fast path). The key border is four
    1 px fills instead of a stroked rectangle. The analog fill is whole rows
    plus one partial row at alpha = coverage, the same pixels an
    anti-aliased fractional edge produces, so slow presses still move
    smoothly instead of in whole-pixel steps.
  - Solid brushes are cached per target by RGBA8; the impact-flash gradient
    uses 64 quantized opacity levels.
  - Labels use DirectWrite (Segoe UI, DPI-scaled 12 px); text layouts and
    formats are cached.
  - Bound icon glyphs are still rasterized once by `RemapIcons_DrawGlyphAA`
    into the existing DIB cache and uploaded once per target as `ID2D1Bitmap`.
  - Compound (ISO Enter) keys are clipped to their outer contour and the analog
    interior to the inset contour with aliased geometry layers.
- `KeyboardRender_DrawKey(DRAWITEMSTRUCT*)` remains for owner-draw users (layout
  editor, `WM_PRINTCLIENT`, fallback when Direct2D is unavailable). It uses the
  same Direct2D code through an `ID2D1DCRenderTarget` (premultiplied, cleared
  transparent, plus a GDI clip on the notch) so neighbour pixels in the notch
  are preserved; the simulator self-test checks this.
- If the canvas cannot be created the page logs
  `[ui.keyboard] canvas=owner-draw fallback` and keys owner-draw as before.

## All-keys animation with the curve editor (2026-10-01)

Owner scenario: on Configuration, the global curve's start point held at the
left edge and moved up and down, so every key's fill changes at once.
Measured with the perf curve sweep (`--halljoy-perf-log`, registered message
`HallJoy.Perf.CurveSweep.v1`, 6 s) and the UI stack sampler
(`--halljoy-perf-stacks`, `tools/analyze_perf_stacks.py`) on the owner's PC
(RTX 3070, 199 Hz).

| | Before | After |
| --- | --- | --- |
| Canvas frames | 145–175 fps | 200 fps (display rate) |
| Refreshes without a new frame | ~12 % | 3 of 1194 |
| Canvas frame | 2.7 ms | 1.6 ms |
| Configuration page paint | 4.3 ms | 1.5 ms |
| UI thread busy | ~70 % | 39 % |

Changes:
1. Vblank-grid frame pacing (see Architecture).
2. Configuration page `WM_PAINT`:
   - a persistent 32 bpp top-down DIB back buffer instead of a new client-sized
     compatible bitmap per paint;
   - all composition clipped to `ps.rcPaint`; only that rectangle is copied
     out, so pixels outside it are never read;
   - GDI+ draws the graph overlay straight into DIB memory instead of reading
     back a device bitmap (overlay cost about 2.5× lower).
3. `WinUtil_GetDpiForWindowCompat` resolves `GetDpiForWindow` once instead
   of `GetModuleHandleW` + `GetProcAddress` on every call (hundreds per frame
   in layout code).
4. Pixel-aligned key fills and border (see above).

The curve change itself is cheap: settings setters publish a curve generation
and the engine recomputes on its next tick.

Backups: `.local/backups/keyboard_subpages.cpp.before-config-paint-2026-10-01`,
`keyboard_canvas.cpp.before-vblank-grid-2026-10-01`.

## Visual changes (intentional)

- Label font: DirectWrite Segoe UI instead of GDI+ `DEFAULT_GUI_FONT`.
- Analog fill edge is sub-pixel.
- Selection glow follows the compound key contour.
- "Mouse Bindings" title uses the same text renderer.

Visual evaluation is the owner's; the agent verified build and automated checks
only.

## Backups

`.local/backups/keyboard_render-before-d2d-2026-10-01.cpp/.h`,
`keyboard_page_main-before-d2d-2026-10-01.cpp`,
`keyboard_page_main-before-canvas-integration-2026-10-01.cpp`,
`keyboard_ui-before-d2d-2026-10-01.cpp`,
`keyboard_ui-before-canvas-integration-2026-10-01.cpp`.

## Verification (agent, 2026-10-01)

- `tools/build_release.ps1`: PASS, including `PROFILE_TRANSACTION_WINDOWS_TEST`
  (`layout_editor_production_events=PASS` covers the compound-key notch
  preservation through the Direct2D DC path and `KeyboardUI_TestCompoundButtonPaint`).
  EXE SHA-256 `f8bf2286a3056011c76275fb5348b1da28e15fed97f9eeb0524ed7120eede7a8`.
- `tools/run_native_backend_checks.py --require-compiler`: PASS.
- Render static audits updated (`main_keyboard_input_static_audit.py` now checks
  the Direct2D contour layers): PASS.
- Analog simulator (`run_analog_simulator.ps1 -IsolateSyntheticInput`): the app
  started, logged `[ui.keyboard] canvas=direct2d`, ran 8 s and exited with 0.
  The runner then failed on its own check for `HallJoyStabilityTrace.log`, which
  the app no longer writes under that name (the last such file is from
  2026-09-13); this is a runner issue unrelated to rendering.
- Not done by the agent: visual review and FPS measurement with all keys
  pressed (owner).
