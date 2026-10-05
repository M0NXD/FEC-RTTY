[CmdletBinding()]
param(
    [string]$GuiExe,
    [string]$EmitterExe,
    [ValidateSet('winmm','portaudio')][string]$Backend = 'winmm',
    [int]$InputDevice = 0,
    [int]$OutputDevice = 1,
    [string]$EvidenceDir
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (-not $GuiExe) { $GuiExe = Join-Path $project 'source/build-gui/fectty-gui.exe' }
$GuiExe = (Resolve-Path -LiteralPath $GuiExe).Path
if (-not $EmitterExe) { $EmitterExe = Join-Path (Split-Path -Parent $GuiExe) 'fectty-noise-prefix-bench.exe' }
$EmitterExe = (Resolve-Path -LiteralPath $EmitterExe).Path
if (-not $EvidenceDir) { $EvidenceDir = Join-Path $project 'evidence/audit-20261004-r11' }
New-Item -ItemType Directory -Force -Path $EvidenceDir | Out-Null
$EvidenceDir = (Resolve-Path -LiteralPath $EvidenceDir).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
foreach ($scenario in @('clean','continuous')) {
    $report = Join-Path $EvidenceDir "noise-$stamp-$Backend-$scenario-rx.json"
    $snapshot = Join-Path $EvidenceDir "noise-$stamp-$Backend-$scenario-rx.png"
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $GuiExe
    $info.WorkingDirectory = Split-Path -Parent $GuiExe
    $info.Arguments = '--autostart --rx '+$Backend+':'+$InputDevice+' --tx '+$Backend+':'+$OutputDevice+' --center-hz 1500 --volume 50 --run-seconds 12 --report "'+$report+'" --snapshot "'+$snapshot+'"'
    $info.UseShellExecute = $false
    $rx = [Diagnostics.Process]::Start($info)
    try {
        Start-Sleep -Milliseconds 1500
        & $EmitterExe "${Backend}:$OutputDevice" $scenario
        $txExit = $LASTEXITCODE
        if (-not $rx.WaitForExit(18000)) { throw 'Noisy-prefix receiver timeout' }
        $result = Get-Content -LiteralPath $report -Raw -Encoding UTF8 | ConvertFrom-Json
        if ($txExit -ne 0 -or $rx.ExitCode -ne 0 -or $result.received_text -cne 'PREFIX TEST' -or
            $result.frames_ok -ne 2 -or $result.acquisitions -ne 1 -or $result.crc_failures -ne 0 -or
            $result.sequence_gaps -ne 0 -or $result.dropped_samples -ne 0 -or $result.capture_interruptions -ne 0 -or
            $result.waterfall_rows -le 0 -or [Math]::Abs([double]$result.waterfall_peak_hz-1500) -gt 100) {
            throw "Noise-prefix regression failed: $report"
        }
        Write-Output "$Backend/${scenario}: exact=True frames=2 crc=0 gaps=0 drops=0 interruptions=0 tx_exit=$txExit rx_exit=$($rx.ExitCode) report=$report"
    } finally {
        if (-not $rx.HasExited) { $rx.Kill(); $rx.WaitForExit() }
        $rx.Dispose()
    }
}
