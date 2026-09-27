# Configuration/tester spacing and editor pan

Owner reported excessive top space on Configuration and Gamepad Tester and asked
to move layout editor camera panning from middle-drag to right-drag.

Tester reserved a 36-DPI-scaled-pixel toolbar even with the camera test compiled
out. Its height now follows the existing feature flag. Configuration reserved a
28-pixel override row even when no key was selected. The retained global view
now collapses that row using one shared offset for control rectangles, graph,
hit regions, lower settings, drag hints and scroll extent. Selecting a key retains
the override row. Legacy child-control layout is unchanged.

Editor right-drag now pans with capture. Right-click without movement still
deletes a guide; releasing after a pan cannot delete the guide beneath the
cursor. Middle-drag no longer pans, wheel zoom remains, and editor hints updated.
No layout document changes are caused by camera movement.

Validation: production-linked editor gesture and configuration selection/geometry
regressions PASS, including right-click guide removal and middle-button rejection.
Layout editor static audit PASS. Ordinary Release and six executable gates PASS;
previously running HallJoy restored automatically. No visual run or publication.
Logs: .local/ui-spacing-pan-profile-20260927.log,
.local/ui-spacing-pan-release-20260927.log.
Backup: .local/backups/ui-spacing-pan-20260927-*.zip.
