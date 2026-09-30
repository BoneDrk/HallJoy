# HallJoy 1.6.6

- Added MADLIONS MAD68 HE V2 Flagship support.
- Reworked shortcuts:
  - Block Bound Keys and Pause/Resume use one system. It accepts both digital
    and analog presses, including keys bound to the gamepad, and chords that
    mix them (for example, a digital Ctrl with an analog key).
  - Pause/Resume can use one toggle key or separate Pause and Resume keys.
  - Shortcuts also work over remote access and with any connected keyboard.
- Fixed ATK Hex80, MADLIONS MAD68 HE V2 Flagship, AULA W669-based keyboards
  (including Redragon K673/K617) and Addressed-protocol keyboards (including
  IPI/QBZ) stopping after Pause/Resume.
- Fixed a false "connection unstable" warning at startup.
- Added a button that resets a key's curve to a straight line without changing
  its deadzones, and a Revert button that discards unsaved preset changes.
- Improved AULA HERO keyboard connection reliability. The log now records why
  a HERO keyboard is not used.
- AJAZZ AK820 MAX RGB is now listed under its retail name, AK820 MAX HE (wired,
  RGB).
- Fixed several reliability issues found in a code review. They include
  disconnect handling for several native keyboards, Input Overlay access
  restricted to this computer and a more complete factory reset.

[Full keyboard list and support statuses](https://docs.google.com/spreadsheets/d/1ueQ4labXpuBOmGjCkUcJllNJ68Jzx4py2MuQm-p-R7c/edit#gid=0).
