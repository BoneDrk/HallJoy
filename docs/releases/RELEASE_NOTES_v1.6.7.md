# HallJoy 1.6.7

- Added support for MCHOSE Jet 75 II, Everglide SU75 Pro and Royal Kludge
  RK68 HE.
- Added experimental support for more keyboards:
  - ATK keyboards on the Hex80 platform: EDGE 60 HE, EDGE 63 HE, EDGE 75 HE,
    60 RX, 68 RX, 68 V2 Pro, 68 V3, RS63 Air and the RS6 and RS7 families.
  - MCHOSE: Ace 60 Pro, Ace 60X, Ace 68, Ace 68 Air, Ace 68 V2, Ace 68 Turbo
    8K, Ace 75 8K, Jet 75 (I revision), Mix 87 (I revision) and Zero75X.
- Added built-in layouts for AJAZZ AK820 MAX HE, MCHOSE Mix 87 III, NuPhy
  Field75 HE, SteelSeries Apex Pro and the newly supported keyboards. Most of
  them are selected automatically.
- MCHOSE keyboards no longer show a false "no analog input" warning before the
  first key press, and no longer disconnect when Windows reports a device
  change.
- The support log now records each keyboard's USB product name, so the exact
  model is visible in the log.
- The keyboard view is now drawn with Direct2D in one pass, paced to the
  display's refresh rate. Pressing many keys at once no longer drops frames,
  and editing a curve no longer slows the keyboard animation.
- Faster startup: the gamepad is ready about twice as fast, and the window
  appears with the keyboard already drawn.
- Fixed the Keychron RGB key and the Wooting Profile/Mode keys updating
  unevenly or looking stuck in the keyboard view. They can now be bound by
  pressing them.
- Fixed a delay of up to 5 seconds in writing HallJoy.log while the log file
  was open in another program.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
