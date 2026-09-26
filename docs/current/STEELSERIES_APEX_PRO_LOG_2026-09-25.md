> Follow-up: owner confirms restart begins with HallJoy. SparkLink generic HID probe sends01 02, which resets the reviewed Apex firmware; fixed alongside native analog. See [implementation and reset evidence](STEELSERIES_APEX_PRO_IMPLEMENTATION_2026-09-25.md). Earlier unknown-cause statements below describe initial log-only triage.

# SteelSeries Apex Pro report — 2026-09-25

Input: owner's Downloads/message (2).txt, HallJoy1.6.3.0 support report.
SHA256151648632b69570dd69bdbe2b05fc77a8b65132d300691440d536d1894eb9847.
663 lines,46762 bytes. Header UTC2026-09-25T16:54:40Z.

Observed: all24 snapshots have analogue_connected=0/devices=0. There are241
device.change notifications (value7) and16 metadata inventories. Device1038:1610
appears in8 inventories and disappears in8; all its listed interfaces disappear
together. OpenRGB device inventory associates this tuple with SteelSeries Apex Pro:
https://gitlab.com/CalcProgrammer1/OpenRGB/-/issues/3744 . Exact firmware is absent.
1038:12B3 stays visible; its model is not established by this report.

UAP exited13 times: two0 and eleven3762832470=0xE0484456. The latter is explicitly
kHostExitDeviceRefresh in analog_host_client.cpp, used by the parent to terminate
and restart discovery after device-change generation changes. It is NOT evidence
of an unhandled crash. No analogue exchange or successful connection recorded.
The report cannot determine why devices disappear: manual reconnects, software,
USB/power issues or discovery behavior remain hypotheses. Do not blame GG or user.

Current native backend registry and bundled UAP AnalogueKeyboard implementation
have no SteelSeries/Apex Pro analog route. This is not an established-protocol
missing-PID alias case. Adding the tuple alone cannot implement support. This
finding does not prove the firmware cannot expose analog; that requires protocol
research using exact Apex generation/firmware/vendor software evidence.

No code/build/publication or support-status change. Existing uninvestigated status
is not changed to unsupported. Follow-up: establish live depth command/stream in
vendor software/firmware and investigate broader hotplug refresh storm handling.
