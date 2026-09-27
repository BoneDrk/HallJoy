# Build replacement and restoration

Owner requires automatic restoration if HallJoy was running before replacement.
No repeated permission is needed. Restore normal/minimized state; never launch
the interactive app hidden. Earlier leave-closed guidance is superseded.

## Cause and correction

The closer used Get-Process.Path, which was empty for the actual running HallJoy.
It therefore falsely reported no instance, allowing replacement without shutdown
and skipping restoration. The new helper uses OpenProcess with
PROCESS_QUERY_LIMITED_INFORMATION and QueryFullProcessImageName, without remote
module enumeration. CIM original executable name selects candidates; exact image
path selects this checkout. Unverifiable identity stops replacement explicitly.
EnumWindows identifies the main WootingVigemGui window even when hidden and sends
WM_CLOSE before the bounded forced-close fallback. Forced close rechecks process
start time and image path. Normal/minimized restoration occurs in finally, also
after replacement failure; immediate launch exit is reported as an error.

## Validation

tools/test_build_replacement.ps1 PASS: unchanged image, missing candidate,
unrelated same-name executable preserved, successful replacement and restart,
locked replacement restores old application, inactive application stays closed,
orphan worker does not launch an interactive app.
Static lifecycle audit PASS. Log: .local/build-lifecycle-20260927.log.

Actual running application PID16652 reproduced empty Get-Process.Path while the
new inspection reported WasRunning=true, Count=5, WindowStyle=Normal and the
correct installed target. This was read-only; the already running app was left
untouched. No claim of a real interactive replacement/visual test is made.

Backup: .local/backups/build-lifecycle-20260927-*.zip.
Only build tools/tests/docs changed; existing installed EXE is unchanged.
No GitHub publication.


## 2026-09-27 elevation mismatch during banner-log installation

Verified HallJoy PID1616 TokenElevation=1, command shell TokenElevation=0;
whoami showed medium integrity and Administrators deny-only. Direct posting of
HallJoy.ExitForBuild.v1 to the verified main window failed with Win32 error5;
forced termination was also denied. This was an integrity boundary, not evidence
of an app deadlock. Show/Taskbar registered messages are allowed across UIPI;
ExitForBuild is not. Do not weaken the window message filter to bypass this.

Owner restarted the host app expecting administrator rights, but command token
remained medium and replacement still failed against PID17044. Standard Windows
RunAs invocation of existing publish_halljoy_build.ps1 then succeeded (exit0),
without changing app code or global privilege/security settings. Installed hash
63a35ca8d98bf9d2a034bc300b9c3c0389eb674b7d1efe89d91c57007305b436 matches candidate;
read-only lifecycle inspection confirms WasRunning=true, Count5, Normal.
Future elevated-target replacements require an elevated publisher; remembered
owner authorization does not itself elevate the OS token.
