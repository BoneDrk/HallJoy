# Support log: USB product names (2026-10-02)

Owner request after the Everglide SU75 Pro log: the log showed only `1CA6:3002`,
so the keyboard model had to be guessed. Each `hid.candidate` row of the support
log inventory now also has the product name:

    hid.candidate vid=1CA6 pid=3002 interface=00 name="..." metadata_only=1

- Source: `DEVPKEY_Device_BusReportedDeviceDesc` (the USB product string from the
  keyboard firmware, the same text the vendor drivers show) of the HID devnode;
  if it is empty, the parent USB node (composite interface or device).
- Windows metadata only (`SetupDiGetDevicePropertyW` / `CM_Get_DevNode_PropertyW`);
  the device is not opened, no reports are sent.
- Sanitized: control characters, `"` and `\` become `_`; at most 48 characters;
  UTF-8. Empty `name=""` when Windows has no value.
- Not logged (unchanged): serials, instance paths, user-editable friendly names.
- Code: `support_log.cpp` (`BusReportedName`, `Inventory`); test
  `support_log_windows_test.cpp` requires the `name=` field on every row;
  contract: `docs/development/SUPPORT_DIAGNOSTICS_CONTRACT.md`.
- Backup: `.local/backups/support_log.cpp.before-product-names-2026-10-02`.

Caveat: the product string is chosen by the manufacturer. It can be generic
(for example a platform name instead of the retail model), so it is evidence,
not proof, of the retail model.
