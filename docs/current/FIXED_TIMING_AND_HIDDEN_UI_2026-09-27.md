# Fixed scheduling and hidden UI — 2026-09-27

Owner requested removal of Polling rate and UI refresh interval, including the
effect of previously saved values. Tray minimization and game profiles are a
separate next task, not implemented here. No publication requested.

Removed both retained sliders, compatibility HWNDs, hit targets, handlers and
spacing. Runtime uses the former fastest/default1ms scheduling requests; protocol
limits and Windows timer minimum still apply. This is not a claim of1000Hz
hardware samples or1000FPS. Legacy setters are no-ops, getters return the fixed
policy. Old INI values are neither parsed nor numerically validated, including
malformed values; writers remove both keys. Legacy file recognition still accepts
the presence of PollingMs as historical schema evidence, without applying it.

Minimized/hidden main window unpublishes its preview HWND, stopping worker-side
repaint hints. Pending dirty bits remain coalesced to latest values. Main timer
continues nonvisual maintenance at100ms without changing the realtime input loop.
Visibility restoration re-enables foreground cadence and invalidates the current
view. Automatic layout state and communication health remain maintained while
hidden; layout UI notifications are deferred until visible. Native pad monitor
is notified immediately of visibility changes.

Keyboard preview, mouse visualizations, layout/remap animations and page timers
check top-level minimization as well as child visibility. Hidden tabs do not
perform their periodic visual work. Overlay server and input/gamepad processing
remain independent. This does not stop all background work; it stops visual
updates and preserves required engine maintenance.

Regression coverage: legacy/bundled profiles, malformed obsolete values, no-op
setters and removal on save; transaction coherence uses a still-persisted field.
Actual Win32 child/root hidden/minimized/restored checks run on a private test
desktop, with no input backend initialization or owner-visible visual run.

Validation PASS: full static audits, production-linked profile tests (including
obsolete_timing_ignored), private-desktop visibility checks and18 startup recovery
scenarios. Ordinary Release and six linked executable gates PASS.

Build: build/bin/Release/x64/HallJoy.exe; SHA256
bd42a5eba5dfa9f9561f1cede57f69cd61064928ade57b48a271c62050d47b52.
Logs: .local/timing-ui-static-20260927.log, timing-ui-profile-20260927.log,
timing-ui-release-20260927.log. No GitHub publication or visual evaluation.
Old interactive process692 and its verified children closed after normal close
did not complete. HallJoy left closed; no hidden interactive relaunch.

Backups:
.local/backups/timing-ui-20260927-*.zip.
