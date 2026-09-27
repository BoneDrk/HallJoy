# Keychron firmware licensing

The modified firmware derives from Keychron/QMK and is licensed under
GPL-2.0-or-later. Preserve upstream notices and provide complete corresponding
firmware source, build instructions and required submodules when distributing
firmware binaries. COPYING contains GPL version 2.

On 2026-09-27 the owner authorized an additional GPL-2.0-or-later option for
these HallJoy-authored shared headers in src/HallJoyProject/HallJoy:

- `keychron_hj_protocol.h`
- `keychron_onboard_session.h`
- `keychron_onboard_profile.h`
- `keychron_onboard_compact.h`
- `keychron_onboard_sparse.h`
- `keychron_onboard_precision.h`
- `keychron_onboard_curve.h`
- `keychron_onboard_mapper.h`

Each listed header may be used under AGPL-3.0-only OR GPL-2.0-or-later, at the
recipient's choice. The grant covers these files, not the rest of HallJoy,
its Windows application or third-party components. It provides a compatible
license for code shared with the modified firmware. No private-use or
field-of-use restriction is added to either open-source license.

HallJoy's other licensing and commercial terms remain unchanged. The source
patch preserves Keychron's original notice in usb_descriptor_override.c.
The full upstream firmware dependency license obligations still apply.
