# Layout editor rendering performance

Owner reports approximately 20 FPS during right-drag panning and requests no
artificial frame cap. No 20-FPS throttle was found. The 16ms timer samples live
analog/bind status; it did not pace mouse invalidations. The old renderer rebuilt
key text/font objects and used a newly allocated compatible bitmap on each paint.

An offscreen production renderer benchmark on the private test desktop, 82 keys,
1164x721 client, 60 moving-camera frames measured 54.111ms mean before changes.
Caching labels alone measured 34.329ms; a 32-bit DIB measured 13.405ms. Subsequent
direct-buffer/ruler variants measured 13.971, 15.179 and 16.326ms. These are CPU
render costs including a GDI flush, not measured displayed FPS or input latency.
The benchmark does not include desktop composition or monitor presentation.

Implementation:
- Reusable top-down 32-bit editor buffer, resized only with the client area;
  canvas GDI+ renders directly into its pixels. Allocation failure falls back to
  direct painting. Resources are released with the editor state.
- Per-key antialiased text raster cache keyed by label, dimensions, font size and
  color. Translation reuses it; edits, selection and zoom rebuild affected entries.
- Offscreen key culling, and clipping regions only for actual analog fill.
- Integer ruler ticks use GDI with preblended colors, avoiding per-tick alpha work.
- Panning invalidates only canvas/rulers and calls UpdateWindow after camera
  movement. It does not wait for the live-analog timer. No new timer or FPS cap.

Production-linked profile/editor gesture tests and 18 recovery cases PASS.
Layout-editor static audit PASS. One intermediate run failed an unrelated overlay
editing event test before reaching the editor benchmark; a rerun and subsequent
build/test runs passed. No manual visual evaluation was performed.
Logs: .local/editor-render-before.log, editor-render-after.log,
editor-render-dib.log, editor-render-final.log, editor-render-direct.log,
editor-render-optimized.log, editor-render-release.log. Per-run benchmark text is
in the HJProfileTest data root named in each log. Benchmark is simulator-only.
Backup: .local/backups/editor-render-20260927-*.zip. No publication.

## Follow-up after owner rejected 13–16ms as an adequate result

Stage profiling found grid=11.201ms, keys=3.001ms, rulers=0.461ms, total14.664ms.
Replaced hundreds of blended line calls with a world-anchored coverage raster.
Axis coverage preserves major/minor spacing, zoom fading and subpixel motion;
8-bit alpha uses a precomputed compositing table. Identical image rows are copied
instead of recomputed. The grid cache invalidates on dimensions, camera phase,
scale and grid parameters; pixels transfer directly into the persistent DIB.
No FPS cap or frame timer added.

Final comparable offscreen result: 82 keys,1164x721,60 camera frames:
**3.921ms total =0.476ms grid +3.000ms keys +0.445ms rulers**.
This remains a rendering-cost benchmark, not measured displayed FPS. Keys are
now the largest remaining render cost. Owner evaluates actual visual smoothness.
Grid regression covers negative world coordinates, exact one-pixel translation,
cache reuse, zoom invalidation and resize; production-linked editor/profile tests
and18 recovery cases PASS. Layout editor static audit PASS.
Evidence: .local/editor-render-grid-final.log; benchmark at
C:/Users/PC/AppData/Local/Temp/HJProfileTest-bb1fe4120ac94e47bd936938192b1a99/editor-render-benchmark.txt.
Ordinary Release evidence: .local/editor-grid-release.log. No publication.
