[CmdletBinding()]
param(
    [string]$GuiExe,
    [string]$OutputDevice = 'winmm:1',
    [string]$EvidenceDir
)
$ErrorActionPreference = 'Stop'
$project = Split-Path -Parent $PSScriptRoot
if (-not $GuiExe) { $GuiExe = Join-Path $project 'source/build-gui/fectty-gui.exe' }
$GuiExe = (Resolve-Path -LiteralPath $GuiExe).Path
if (-not $EvidenceDir) { $EvidenceDir = Join-Path $project 'evidence/audit-20261004-r11' }
New-Item -ItemType Directory -Path $EvidenceDir -Force | Out-Null
$EvidenceDir = (Resolve-Path -LiteralPath $EvidenceDir).Path
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'

function Quote-WindowsArgument([string]$Value) {
    $escaped = [regex]::Replace($Value, '(\\*)"', '$1$1\"')
    $escaped = [regex]::Replace($escaped, '(\\+)$', '$1$1')
    return '"' + $escaped + '"'
}
# Visible windows; only the explicitly selected virtual-cable output is used.
# Verify current device names/indices before calling this hardware bench test.
$cases = @(
    @{ Name='idle'; Args=@('--run-seconds','1'); Exit=0; Success=$false },
    @{ Name='not-started'; Args=@('--autostart','--rx','none','--tx',$OutputDevice,'--send','NOT STARTED','--run-seconds','1'); Exit=2; Success=$false },
    @{ Name='cancelled'; Args=@('--autostart','--rx','none','--tx',$OutputDevice,'--send',('X'*500),'--quit-after-send','--run-seconds','3'); Exit=2; Success=$false },
    @{ Name='completed'; Args=@('--autostart','--rx','none','--tx',$OutputDevice,'--send','EXIT OK','--quit-after-send','--run-seconds','12'); Exit=0; Success=$true }
)
foreach ($case in $cases) {
    $report = Join-Path $EvidenceDir "exit-$stamp-$($OutputDevice.Replace(':','-'))-$($case.Name).json"
    $arguments = $case.Args + @('--volume','50','--report',$report)
    $info = New-Object Diagnostics.ProcessStartInfo
    $info.FileName = $GuiExe
    $info.WorkingDirectory = Split-Path -Parent $GuiExe
    $info.UseShellExecute = $false
    $info.Arguments = (($arguments | ForEach-Object { Quote-WindowsArgument $_ }) -join ' ')
    $process = [Diagnostics.Process]::Start($info)
    try {
        if (-not $process.WaitForExit(20000)) { throw "Exit test timed out: $($case.Name)" }
        $result = Get-Content -Raw -LiteralPath $report -Encoding UTF8 | ConvertFrom-Json
        if ($process.ExitCode -ne $case.Exit -or $result.tx_success -ne $case.Success) {
            throw "Exit test failed: $($case.Name), code=$($process.ExitCode), tx_success=$($result.tx_success), report=$report"
        }
        if ($case.Name -eq 'cancelled' -and ($result.tx_error -ne 'Transmission cancelled' -or $result.draft_text -cne ('X'*500))) {
            throw "Cancellation did not preserve the full draft/error: $report"
        }
        if ($case.Name -eq 'completed' -and ($result.tx_error -or $result.draft_text)) {
            throw "Successful send did not clear the completed draft: $report"
        }
        Write-Output "$($case.Name): exit=$($process.ExitCode), tx_success=$($result.tx_success), report=$report"
    } finally {
        if (-not $process.HasExited) { $process.Kill(); $process.WaitForExit() }
        $process.Dispose()
    }
}
