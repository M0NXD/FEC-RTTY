[CmdletBinding()]
param(
    [string]$GuiExe,
    [ValidateSet('winmm', 'portaudio')][string]$Backend = 'winmm',
    [int]$InputDevice = 0,
    [int]$OutputDevice = 1,
    [double]$CenterHz = 1500,
    [ValidateRange(1,100)][int]$Volume = 50,
    [ValidateRange(8,60)][int]$ReceiveTailSeconds = 20,
    [switch]$SplitOffsets,
    [switch]$RetuneRx,
    [string]$EvidenceDir
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (-not $GuiExe) {
    $GuiExe = Join-Path $project 'gui\fectty-gui.exe'
    if (-not (Test-Path -LiteralPath $GuiExe)) { $GuiExe = Join-Path $project 'source\build-gui\fectty-gui.exe' }
}
$GuiExe = (Resolve-Path -LiteralPath $GuiExe).Path
$evidence = if ($EvidenceDir) { $EvidenceDir } else { Join-Path $project 'evidence\audit-20261004' }
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$evidence = (Resolve-Path -LiteralPath $evidence).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

function Quote-WindowsArgument([string]$Value) {
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
function Start-GuiBench([string[]]$Arguments) {
    $startInfo = New-Object System.Diagnostics.ProcessStartInfo
    $startInfo.FileName = $GuiExe
    $startInfo.Arguments = (($Arguments | ForEach-Object { Quote-WindowsArgument $_ }) -join ' ')
    $startInfo.WorkingDirectory = Split-Path -Parent $GuiExe
    $startInfo.UseShellExecute = $false
    return [System.Diagnostics.Process]::Start($startInfo)
}

# Both windows stay visible while testing. The single installed cable is used
# sequentially in the two directions; each direction uses a distinct payload.
$messages = @("1234567$([char]0xe9)`n`nA TO B`n", "B TO A $([char]0xe9)`n`n")
for ($direction = 0; $direction -lt $messages.Count; $direction++) {
    $message = $messages[$direction]
    $byteCount = [Text.Encoding]::UTF8.GetByteCount($message)
    $frames = [int][Math]::Ceiling($byteCount / 8.0)
    $duration = 0.5 + $frames * 2.36
    # RX processes captured samples asynchronously. A fixed short timer can
    # abandon queued samples at shutdown even after playback has completed.
    # This bounded bench tail is not a fix for the application's Stop/drain risk.
    $seconds = [int][Math]::Ceiling($duration + $ReceiveTailSeconds)
    $prefix = Join-Path $evidence "$Backend-$stamp-direction-$direction"
    $rxReport = "$prefix-rx.json"
    $txReport = "$prefix-tx.json"
    $common = @('--center-hz', $CenterHz.ToString([Globalization.CultureInfo]::InvariantCulture), '--volume', "$Volume")
    $rxTuning = @(); $txTuning = @()
    if ($SplitOffsets -or $RetuneRx) {
        $initialRx = if ($RetuneRx) { $CenterHz-400 } else { $CenterHz }
        $rxTuning = @('--rx-center-hz', "$initialRx", '--tx-center-hz', "$($CenterHz+300)")
        $txTuning = @('--rx-center-hz', "$($CenterHz-300)", '--tx-center-hz', "$CenterHz")
        if ($RetuneRx) { $rxTuning += @('--retune-rx-hz', "$CenterHz", '--retune-after', '2') }
    }
    $rx = $null; $tx = $null
    try {
        $rx = Start-GuiBench (@('--autostart', '--rx', "${Backend}:$InputDevice", '--tx', "${Backend}:$OutputDevice", '--run-seconds', "$seconds", '--report', $rxReport, '--snapshot', "$prefix-rx.png") + $common + $rxTuning)
        Start-Sleep -Milliseconds 1000
        $tx = Start-GuiBench (@('--autostart', '--rx', 'none', '--tx', "${Backend}:$OutputDevice", '--send', $message, '--quit-after-send', '--run-seconds', "$seconds", '--report', $txReport) + $common + $txTuning)
        Write-Output "Visible $Backend direction $direction GUI RX=$($rx.Id) TX=$($tx.Id) expected=$byteCount bytes"
        if (-not $tx.WaitForExit(($seconds + 5) * 1000)) { throw 'GUI TX did not finish' }
        if (-not $rx.WaitForExit(($seconds + 5) * 1000)) { throw 'GUI RX did not finish' }
        $received = Get-Content -Raw -LiteralPath $rxReport -Encoding UTF8 | ConvertFrom-Json
        $sent = Get-Content -Raw -LiteralPath $txReport -Encoding UTF8 | ConvertFrom-Json
        $exact = $received.received_text -ceq $message
        Write-Output "direction=$direction exact=$exact tx_exit=$($tx.ExitCode) rx_exit=$($rx.ExitCode) frames=$($received.frames_ok) crc=$($received.crc_failures) gaps=$($received.sequence_gaps) drops=$($received.dropped_samples) acquisitions=$($received.acquisitions) center=$($received.center_hz)"
        if ($tx.ExitCode -ne 0 -or $rx.ExitCode -ne 0 -or -not $sent.tx_success -or -not $exact -or
            $received.frames_ok -ne $frames -or $received.crc_failures -ne 0 -or $received.sequence_gaps -ne 0 -or $received.dropped_samples -ne 0 -or $received.capture_interruptions -ne 0) {
            throw "GUI direct test failed; reports: $rxReport $txReport"
        }
        if ($received.waterfall_rows -le 0 -or $received.center_hz -ne $CenterHz -or $sent.tx_center_hz -ne $CenterHz) { throw 'Waterfall or actual RX/TX offset assertion failed' }
        if ([Math]::Abs([double]$received.waterfall_peak_hz - $CenterHz) -gt 100) { throw 'Waterfall did not measure the transmitted tone region' }
        if (($SplitOffsets -or $RetuneRx) -and ($received.link_offsets -or $sent.link_offsets -or $received.tx_center_hz -ne $CenterHz+300 -or $sent.center_hz -ne $CenterHz-300)) { throw 'Independent tuning assertion failed' }
    }
    finally {
        foreach ($process in @($tx, $rx)) {
            if ($null -ne $process) {
                if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
                $process.Dispose()
            }
        }
    }
}
