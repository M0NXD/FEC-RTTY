[CmdletBinding()]
param(
    [ValidateSet('short', '20', '50', 'long')]
    [string]$Profile = 'short',
    [int]$InputDevice = 0,
    [int]$OutputDevice = 1,
    [double]$Gain = 0.5,
    [int]$Seconds = 0
)

$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
$source = Join-Path $project 'source'
$gxx = Get-Command g++.exe -ErrorAction SilentlyContinue
if ($null -ne $gxx) {
    $env:Path = "$(Split-Path -Parent $gxx.Source);$env:Path"
}
$exe = Join-Path $project 'bin\fectty-live.exe'
if (-not (Test-Path -LiteralPath $exe)) {
    $exe = Join-Path $source 'build\fectty-live.exe'
}
if (-not (Test-Path -LiteralPath $exe)) {
    throw "fectty-live.exe was not found. Run source\build.bat first or unpack a release into bin."
}

switch ($Profile) {
    'short' { $message = 'HELLO'; $defaultSeconds = 15 }
    '20'    { $message = ((0..19 | ForEach-Object { '{0:D7}X' -f $_ }) -join ''); $defaultSeconds = 75 }
    '50'    { $message = ((0..49 | ForEach-Object { '{0:D7}X' -f $_ }) -join ''); $defaultSeconds = 160 }
    'long'  { $message = ((0..63 | ForEach-Object { '{0:D7}X' -f $_ }) -join ''); $defaultSeconds = 220 }
}
if ($Seconds -le 0) { $Seconds = $defaultSeconds }

$evidence = Join-Path $project 'evidence'
New-Item -ItemType Directory -Force -Path $evidence | Out-Null
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$rxLog = Join-Path $evidence "vb-$Profile-$stamp-rx.log"
$txLog = Join-Path $evidence "vb-$Profile-$stamp-tx.log"
$workDir = $source
$rx = $null

try {
    $rx = Start-Process -FilePath $exe `
        -ArgumentList @('--receive', '--input', "$InputDevice", '--seconds', "$Seconds") `
        -WorkingDirectory $workDir -WindowStyle Hidden -RedirectStandardOutput $rxLog -PassThru
    # Keep the native handle before the process exits. Windows PowerShell 5.1
    # can otherwise return a null ExitCode for a Start-Process child.
    $null = $rx.Handle
    Start-Sleep -Milliseconds 750

    Push-Location $workDir
    try {
        & $exe '--send' $message '--output' "$OutputDevice" '--gain' "$Gain" *> $txLog
        $txExit = $LASTEXITCODE
    }
    finally {
        Pop-Location
    }
    if ($txExit -ne 0) {
        throw "Transmitter exited with code $txExit. See $txLog"
    }

    $rx.WaitForExit()
    $rxExitCode = $rx.ExitCode

    $lines = @(Get-Content -LiteralPath $rxLog)
    $decoded = (($lines | Where-Object { $_.StartsWith('RX: ') } | ForEach-Object { $_.Substring(4) }) -join '')
    $exact = $decoded -ceq $message
    $expectedFrames = [int][Math]::Ceiling($message.Length / 8.0)
    $framesOk = $lines -contains "Frames valid: $expectedFrames"
    $clean = ($lines -contains 'CRC failures: 0') -and
             ($lines -contains 'Sequence gaps: 0') -and ($lines -contains 'Dropped samples: 0')

    Write-Output "Executable: $exe"
    Write-Output "Input device: $InputDevice; output device: $OutputDevice; profile: $Profile"
    Write-Output "Expected bytes: $($message.Length); decoded bytes: $($decoded.Length)"
    $exactText = if ($exact) { 'yes' } else { 'no' }
    Write-Output "Exact text: $exactText"
    Write-Output "Receiver exit code: $rxExitCode"
    Write-Output "RX log: $rxLog"
    Write-Output "TX log: $txLog"
    Get-Content -LiteralPath $txLog
    Get-Content -LiteralPath $rxLog

    if (-not $exact -or -not $framesOk -or -not $clean -or $null -eq $rxExitCode -or $rxExitCode -ne 0) {
        exit 3
    }
    exit 0
}
finally {
    if ($null -ne $rx) {
        $rx.Refresh()
        if (-not $rx.HasExited) {
            Stop-Process -Id $rx.Id -Force -ErrorAction SilentlyContinue
        }
    }
    foreach ($capture in @('fectty-live-rx.wav', 'fectty-live-tx.wav')) {
        $capturePath = Join-Path $workDir $capture
        if (Test-Path -LiteralPath $capturePath) {
            Copy-Item -LiteralPath $capturePath -Destination (Join-Path $evidence "vb-$Profile-$stamp-$capture") -Force
        }
    }
}
