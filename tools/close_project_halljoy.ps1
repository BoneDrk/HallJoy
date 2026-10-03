[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$TargetPath,
    [switch]$InspectOnly,
    [ValidateRange(0, 30000)][int]$GraceMs = 5000
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$target = [IO.Path]::GetFullPath($TargetPath)
$checkoutPrefix = [IO.Path]::GetFullPath((Split-Path -Parent $PSScriptRoot)).TrimEnd('\') + '\'
if (-not $target.StartsWith($checkoutPrefix, [StringComparison]::OrdinalIgnoreCase)) {
    throw 'Replacement target must be inside this checkout.'
}
$session = (Get-Process -Id $PID).SessionId
. (Join-Path $PSScriptRoot 'halljoy_process_identity.ps1')
function Get-TargetProcesses {
    # CIM preserves the original name even if a mapped EXE was renamed by replacement.
    foreach ($info in @(Get-CimInstance Win32_Process -Filter "SessionId = $session" | Where-Object {
        $_.Name -eq [IO.Path]::GetFileName($target) -and $_.ProcessId -ne $PID
    })) {
        try { $image = [HallJoyBuild.ProcessIdentity]::ImagePath($info.ProcessId) }
        catch {
            if (Get-Process -Id $info.ProcessId -ErrorAction SilentlyContinue) {
                throw "Cannot verify executable identity for PID $($info.ProcessId); replacement stopped. $_"
            }
            continue
        }
        if ($image.Equals($target, [StringComparison]::OrdinalIgnoreCase)) {
            Get-Process -Id $info.ProcessId -ErrorAction SilentlyContinue
        }
    }
}
$running = @(Get-TargetProcesses)
$interactive = @($running | Where-Object {
    if ([HallJoyBuild.ProcessIdentity]::MainWindow($_.Id) -ne [IntPtr]::Zero) { return $true }
    $info = Get-CimInstance Win32_Process -Filter "ProcessId = $($_.Id)" -ErrorAction SilentlyContinue
    # Normal app invocation has no internal child-role arguments, even in tray.
    if ($info -and -not $info.CommandLine -and $info.ParentProcessId -notin $running.Id) {
        throw "Cannot identify role of PID $($_.Id); replacement stopped."
    }
    $info -and $info.CommandLine -and $info.CommandLine -notmatch '--'
})
$windowStyle = 'Normal'
if ($interactive.Count -eq 1 -and [HallJoyBuild.ProcessIdentity]::IsIconic([HallJoyBuild.ProcessIdentity]::MainWindow($interactive[0].Id))) {
    $windowStyle = 'Minimized'
}
$state = [pscustomobject]@{ WasRunning = ($interactive.Count -gt 0); Count = $running.Count; Target = $target; WindowStyle = $windowStyle }
if ($InspectOnly -or $running.Count -eq 0) { return $state }
foreach ($process in $interactive) {
    if (-not $process.HasExited) { $null = [HallJoyBuild.ProcessIdentity]::RequestClose($process.Id) }
}
$deadline = [DateTime]::UtcNow.AddMilliseconds($GraceMs)
foreach ($process in $running) {
    $remaining = [Math]::Max(0, [int]($deadline - [DateTime]::UtcNow).TotalMilliseconds)
    if (-not $process.HasExited -and $remaining -gt 0) { $null = $process.WaitForExit($remaining) }
}
$isAdmin = ([Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole(
    [Security.Principal.WindowsBuiltInRole]::Administrator)
foreach ($process in @(Get-TargetProcesses)) {
    if ($process.HasExited) { continue }
    $verifiedStart = $process.StartTime
    $fresh = Get-Process -Id $process.Id -ErrorAction SilentlyContinue
    if ($fresh -and $fresh.StartTime -eq $verifiedStart -and
        [HallJoyBuild.ProcessIdentity]::ImagePath($fresh.Id).Equals($target, [StringComparison]::OrdinalIgnoreCase)) {
        try { Stop-Process -InputObject $fresh -Force -ErrorAction Stop }
        catch {
            # HallJoy running as administrator ignores an unelevated build (older
            # builds also drop the exit message). Repeat the same verified close
            # elevated once; Windows shows a UAC prompt.
            if ($isAdmin) { throw }
            Write-Host 'HallJoy runs as administrator; requesting elevation to close it (UAC prompt)...' -ForegroundColor Yellow
            $arguments = "-NoProfile -ExecutionPolicy Bypass -File `"$PSCommandPath`" -TargetPath `"$target`" -GraceMs $GraceMs"
            $elevated = Start-Process -FilePath 'powershell.exe' -Verb RunAs -WindowStyle Hidden -ArgumentList $arguments -PassThru -Wait
            if ($elevated.ExitCode -ne 0) { throw "Elevated close failed ($($elevated.ExitCode))." }
            break
        }
        if (-not $fresh.WaitForExit(5000)) { throw "Target process $($fresh.Id) did not exit." }
    }
}
if (@(Get-TargetProcesses).Count -ne 0) { throw 'Target restarted during replacement shutdown.' }
return $state
