# Next release (unreleased)

- Added keyboard layouts for AJAZZ AK820 MAX HE (wired, RGB), MCHOSE Mix 87 III,
  NuPhy Field75 HE and SteelSeries Apex Pro. AK820 MAX HE, Mix 87 III and Apex Pro
  select their layout automatically.
- Added experimental support for ATK keyboards that use the Hex80 protocol: EDGE 60 HE,
  EDGE 63 HE, EDGE 75 HE, 60 RX, 68 RX, 68 V2 Pro, 68 V3, RS6, RS6 Air, RS6 Cube, RS6 Ultra,
  RS6 Ultra+, RS6+, RS63 Air, RS7, RS7 Air, RS7 Turbo, RS7 V2 and RS7 V2 Ultra, with automatic layouts.
- Added support for MCHOSE Jet 75 II, with an automatic layout.
- Fixed the Keychron RGB key and Wooting Profile/Mode keys updating unevenly or appearing stuck in
  the keyboard view, and allowed them to be bound by pressing.
- Rewrote the keyboard view rendering on Direct2D: the whole keyboard is drawn in one pass paced
  to the display refresh, so pressing many keys at once no longer drops frames. Key labels use
  DirectWrite and the press fill moves smoothly at sub-pixel depth. Frames follow the monitor's
  refresh exactly (one new frame per refresh, also at high refresh rates), and editing the curve
  on the Configuration page no longer slows the keyboard animation.
- Faster startup: the gamepad is ready about twice as fast, and the window appears once with the
  keyboard already drawn instead of an empty keyboard area. Settings and layouts load several
  times faster.
- Fixed a delay of up to 5 seconds in writing HallJoy.log while the log file was open in
  another program, and research records that could stay unwritten until the next snapshot.
