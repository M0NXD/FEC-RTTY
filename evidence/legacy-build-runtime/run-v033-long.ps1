$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $MyInvocation.MyCommand.Path
$legacyToolchain = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'Microsoft\WinGet\Packages\BrechtSanders.WinLibs.POSIX.UCRT_Microsoft.Winget.Source_8wekyb3d8bbwe\mingw64\bin'
$env:Path = $legacyToolchain + ';' + $env:Path
$bin = Join-Path $root 'build\fectty-live.exe'
$rxLog = Join-Path $root 'v033-long-rx.log'
$txLog = Join-Path $root 'v033-long-tx.log'
Remove-Item -Force -ErrorAction SilentlyContinue $rxLog,$txLog
$rx = Start-Process -FilePath $bin -ArgumentList @('--receive','--input','0','--seconds','200') -WorkingDirectory $root -RedirectStandardOutput $rxLog -PassThru
Start-Sleep -Milliseconds 750
$message = ('01234567' * 64)
& $bin '--send' $message '--output' '1' '--gain' '0.5' *> $txLog
$rx | Wait-Process
Write-Output '--- TX ---'
Get-Content $txLog
Write-Output '--- RX ---'
Get-Content $rxLog
 $rxTextLines = Get-Content $rxLog | Where-Object { $_.StartsWith('RX: ') }
 $decoded = ($rxTextLines | ForEach-Object { $_.Substring(4) }) -join ''
 if ($decoded -eq $message) { Write-Output 'Exact text: yes' } else { Write-Output "Exact text: no (decoded bytes=$($decoded.Length), expected bytes=$($message.Length))"; exit 3 }
exit $rx.ExitCode
