[CmdletBinding()]
param(
    [string]$ExePath,
    [Parameter(Mandatory = $true)][string]$OutDir,
    [ValidateRange(1, 10)][int]$Cycles = 3,
    [ValidateRange(3, 60)][int]$IdleSeconds = 10
)
# Local agent research tool (not a tester workflow): real-hardware performance
# profile of the exact build. Startup, idle CPU (shown and minimized),
# pause/resume cycles and graceful exit. HallJoy writes the timeline and
# per-thread CPU samples itself (--halljoy-perf-log), which also works when
# HallJoy runs elevated. This script only drives the phases and records their
# boundaries (ms since launch). It closes a running copy of the same EXE first.
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest

Add-Type -Namespace HallJoyPerf -Name Win32 -MemberDefinition @'
[DllImport("user32.dll", SetLastError = true)] public static extern bool PostMessageW(System.IntPtr hwnd, uint msg, System.IntPtr w, System.IntPtr l);
'@

if (-not $ExePath) { $ExePath = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\bin\Release\x64\HallJoy.exe' }
$exe = [IO.Path]::GetFullPath($ExePath)
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$timeline = Join-Path $OutDir 'perf-timeline.txt'
$phases = Join-Path $OutDir 'perf-phases.txt'
Remove-Item -LiteralPath $timeline, $phases -ErrorAction SilentlyContinue

& (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $exe | Out-Null

$launch = [Diagnostics.Stopwatch]::StartNew()
function Phase([string]$label, [double]$startMs) {
    Add-Content -LiteralPath $phases -Value ("phase {0} {1:F0} {2:F0}" -f $label, $startMs, $launch.Elapsed.TotalMilliseconds) -Encoding ascii
}
function Post([IntPtr]$hwnd, [int]$msg, [int64]$wParam) {
    if (-not [HallJoyPerf.Win32]::PostMessageW($hwnd, [uint32]$msg, [IntPtr]$wParam, [IntPtr]::Zero)) {
        throw ("PostMessage 0x{0:x} failed: {1}" -f $msg, [Runtime.InteropServices.Marshal]::GetLastWin32Error())
    }
}

$process = Start-Process -FilePath $exe -ArgumentList "--halljoy-perf-log=$timeline" -PassThru
$hwnd = [IntPtr]::Zero
while ($launch.Elapsed.TotalSeconds -lt 30) {
    # Child processes (UAP host, ViGEm owner) may own windows of the same
    # class; take the visible main window of the launched process only.
    $process.Refresh()
    $hwnd = $process.MainWindowHandle
    if ($hwnd -ne [IntPtr]::Zero) { break }
    Start-Sleep -Milliseconds 10
}
if ($hwnd -eq [IntPtr]::Zero) { throw 'HallJoy main window did not appear within 30 s.' }
Phase 'window_found' 0

$WM_SYSCOMMAND = 0x0112; $SC_MINIMIZE = 0xF020; $SC_RESTORE = 0xF120
$toggle = 0x8000 + 262   # WM_APP_ENGINE_RUNTIME_TOGGLE

Start-Sleep -Seconds 6
$t = $launch.Elapsed.TotalMilliseconds; Start-Sleep -Seconds $IdleSeconds; Phase 'idle_shown' $t
Post $hwnd $WM_SYSCOMMAND $SC_MINIMIZE
Start-Sleep -Seconds 1
$t = $launch.Elapsed.TotalMilliseconds; Start-Sleep -Seconds $IdleSeconds; Phase 'idle_minimized' $t
Post $hwnd $WM_SYSCOMMAND $SC_RESTORE
Start-Sleep -Seconds 2

for ($i = 0; $i -lt $Cycles; ++$i) {
    Post $hwnd $toggle 0
    Start-Sleep -Seconds 1
    $t = $launch.Elapsed.TotalMilliseconds; Start-Sleep -Seconds 3; Phase "paused_$i" $t
    Post $hwnd $toggle 0
    Start-Sleep -Seconds 1
    $t = $launch.Elapsed.TotalMilliseconds; Start-Sleep -Seconds 4; Phase "active_$i" $t
}

$t = $launch.Elapsed.TotalMilliseconds
& (Join-Path $PSScriptRoot 'close_project_halljoy.ps1') -TargetPath $exe | Out-Null
Phase 'close' $t
for ($wait = 0; $wait -lt 100 -and -not (Test-Path -LiteralPath $timeline); ++$wait) { Start-Sleep -Milliseconds 100 }
if (-not (Test-Path -LiteralPath $timeline)) { throw 'HallJoy did not write the perf timeline.' }
"timeline=$timeline"
"phases=$phases"
