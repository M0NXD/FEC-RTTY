#requires -Version 7.0
[CmdletBinding()]
param([string]$SetupExe, [switch]$TestAudio, [switch]$IsolatedShortcuts)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path -Parent $PSScriptRoot
. (Join-Path $project 'tools/public-paths.ps1')
if (-not $SetupExe) { $SetupExe = Join-Path $project 'releases/v0.46.1-public/FEC-RTTY-0.46.1-Setup-x64.exe' }
$SetupExe = (Resolve-Path $SetupExe).Path
$registryPath = 'Software\Microsoft\Windows\CurrentVersion\Uninstall\FEC-RTTY-M0NXD_is1'
function Read-Installation {
    $base = [Microsoft.Win32.RegistryKey]::OpenBaseKey([Microsoft.Win32.RegistryHive]::CurrentUser,[Microsoft.Win32.RegistryView]::Registry64)
    try {
        $key = $base.OpenSubKey($registryPath)
        if (-not $key) { return $null }
        try { return [ordered]@{ name=$key.GetValue('DisplayName'); version=$key.GetValue('DisplayVersion'); location=$key.GetValue('InstallLocation'); uninstall=$key.GetValue('UninstallString'); quiet=$key.GetValue('QuietUninstallString'); size_kib=$key.GetValue('EstimatedSize') } }
        finally { $key.Dispose() }
    } finally { $base.Dispose() }
}
if (Read-Installation) { throw 'An installed FEC-RTTY already exists. Refusing to replace/uninstall it for acceptance testing.' }
$startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) 'FEC-RTTY - M0NXD'
$desktopShortcut = Join-Path ([Environment]::GetFolderPath('DesktopDirectory')) 'FEC-RTTY - M0NXD.lnk'
if (-not $IsolatedShortcuts -and ((Test-Path $startMenu) -or (Test-Path $desktopShortcut))) { throw 'Pre-existing FEC-RTTY shortcuts found; use -IsolatedShortcuts to test without replacing them.' }
$preservedShortcuts = @()
if ($IsolatedShortcuts) {
    foreach ($file in @((Get-ChildItem -LiteralPath $startMenu -File -Recurse -ErrorAction SilentlyContinue)) + @((Get-Item -LiteralPath $desktopShortcut -ErrorAction SilentlyContinue))) {
        $preservedShortcuts += [ordered]@{ path=$file.FullName; sha256=(Get-FileHash -LiteralPath $file.FullName).Hash }
    }
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
if ($IsolatedShortcuts) { $startMenu = Join-Path ([Environment]::GetFolderPath('Programs')) "FEC-RTTY Acceptance-$stamp" }
$run = Join-Path $PSScriptRoot "build/acceptance-$stamp"
$installDir = Join-Path $run 'Installed FEC-RTTY é'
New-Item -ItemType Directory -Path $run -Force | Out-Null
$shortcutBackupDir = Join-Path $run 'shortcut-backups'
if ($preservedShortcuts.Count) {
    New-Item -ItemType Directory -Path $shortcutBackupDir | Out-Null
    for ($i=0; $i -lt $preservedShortcuts.Count; $i++) {
        $file = $preservedShortcuts[$i]
        $file.backup = Join-Path $shortcutBackupDir "$i.lnk"
        Copy-Item -LiteralPath $file.path -Destination $file.backup
        if ((Get-FileHash -LiteralPath $file.backup).Hash -ne $file.sha256) { throw 'Shortcut backup mismatch; refusing installation.' }
    }
}
$settings = Join-Path ([Environment]::GetFolderPath('LocalApplicationData')) 'FEC-RTTY/FEC-RTTY/fectty.ini'
$settingsBefore = if (Test-Path $settings) { (Get-FileHash $settings).Hash } else { $null }
$driversBefore = @(Get-CimInstance Win32_PnPEntity | Where-Object Name -match 'VB-Audio|VB-CABLE|VBCABLE' | ForEach-Object { $_.DeviceID+'|'+$_.Status } | Sort-Object)
$utf8 = [Text.UTF8Encoding]::new($false)
$results = [Collections.Generic.List[object]]::new()
function Check([bool]$Condition, [string]$Name) {
    $results.Add([ordered]@{ check=$Name; passed=$Condition })
    if (-not $Condition) { throw "Acceptance failed: $Name" }
    Write-Output "PASS: $Name"
}
function Run-Program([string]$Exe,[string[]]$Arguments,[string]$Name,[int]$Timeout=60,[switch]$Visible) {
    $info = [Diagnostics.ProcessStartInfo]::new()
    $info.FileName = $Exe
    foreach ($argument in $Arguments) { $info.ArgumentList.Add($argument) }
    $info.UseShellExecute = $false
    $info.WorkingDirectory = $run
    $info.WindowStyle = if ($Visible) { [Diagnostics.ProcessWindowStyle]::Normal } else { [Diagnostics.ProcessWindowStyle]::Hidden }
    $info.CreateNoWindow = -not $Visible
    $info.RedirectStandardOutput = $true
    $info.RedirectStandardError = $true
    $info.Environment['PATH'] = (Join-Path $env:WINDIR 'System32')+';'+$env:WINDIR
    foreach ($variable in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_PLATFORM','QML2_IMPORT_PATH','QML_IMPORT_PATH')) { $null = $info.Environment.Remove($variable) }
    $process = [Diagnostics.Process]::Start($info)
    try {
        $stdout = $process.StandardOutput.ReadToEndAsync()
        $stderr = $process.StandardError.ReadToEndAsync()
        if (-not $process.WaitForExit($Timeout*1000)) { $process.Kill($true); $process.WaitForExit(); throw "$Name timed out" }
        $output = $stdout.GetAwaiter().GetResult()
        $errors = $stderr.GetAwaiter().GetResult()
        [IO.File]::WriteAllText((Join-Path $run "$Name.log"),$output+"`n"+$errors,$utf8)
        return [ordered]@{ exit=$process.ExitCode; output=$output; errors=$errors }
    } finally { $process.Dispose() }
}
function Check-Manifest {
    $manifest = Get-Content (Join-Path $installDir 'installation-manifest.json') -Raw | ConvertFrom-Json
    foreach ($file in $manifest.files) {
        $path = Join-Path $installDir $file.path
        if (-not (Test-Path -LiteralPath $path) -or (Get-FileHash -LiteralPath $path).Hash -ne $file.sha256) { throw "Installed file differs: $($file.path)" }
    }
    Check ($manifest.app_id -eq 'FEC-RTTY-M0NXD') "Payload identity and all $($manifest.files.Count) installed file hashes"
}
function Uninstall-TestCopy {
    $registration = Read-Installation
    if (-not $registration) { return }
    if ($registration.location.TrimEnd('\') -ne $installDir.TrimEnd('\')) { throw 'Registered installation is not this test copy; refusing uninstall.' }
    $match = [regex]::Match($registration.uninstall,'^"([^"]+)"')
    if (-not $match.Success) { throw 'Unexpected Windows UninstallString.' }
    $uninstaller = $match.Groups[1].Value
    if ([IO.Path]::GetFullPath($uninstaller) -ne (Join-Path $installDir 'unins000.exe')) { throw 'Unexpected registered uninstaller path.' }
    $removed = Run-Program $uninstaller @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',"/LOG=$(Join-Path $run 'uninstall-inno.log')") 'uninstall' 60
    Check ($removed.exit -eq 0) 'Registered Windows uninstaller returns success'
    $deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ((Read-Installation) -and [DateTime]::UtcNow -lt $deadline) { Start-Sleep -Milliseconds 100 }
    Check (-not (Read-Installation)) 'Windows uninstall registration removed'
}
$failure = $null
try {
    $occupied = Join-Path $run 'Occupied unrelated folder'
    New-Item -ItemType Directory -Path $occupied | Out-Null
    [IO.File]::WriteAllText((Join-Path $occupied 'keep.txt'),'Do not overwrite this unrelated folder.',$utf8)
    $refused = Run-Program $SetupExe @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$occupied", "/LOG=$(Join-Path $run 'refused-inno.log')") 'refused-folder'
    Check ($refused.exit -ne 0 -and -not (Read-Installation) -and -not (Test-Path (Join-Path $occupied 'gui'))) 'Unrelated nonempty installation folder rejected before copying'
    Check ((Get-Content (Join-Path $occupied 'keep.txt') -Raw) -eq 'Do not overwrite this unrelated folder.') 'Unrelated existing file preserved'

    $shortcutArgs = if ($IsolatedShortcuts) { @('/TASKS=!desktopicon',"/GROUP=FEC-RTTY Acceptance-$stamp") } else { @('/TASKS=desktopicon') }
    $arguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/SP-',"/DIR=$installDir", "/LOG=$(Join-Path $run 'install-inno.log')") + $shortcutArgs
    $installed = Run-Program $SetupExe $arguments 'install' 120
    Check ($installed.exit -eq 0) 'Offline install succeeds without elevation or reboot'
    $registration = Read-Installation
    Check ($null -ne $registration -and $registration.name -eq 'FEC-RTTY - M0NXD' -and $registration.version -eq '0.46.1') 'Settings/Control Panel uninstall entry has correct product/version'
    Check ($registration.location.TrimEnd('\') -eq $installDir.TrimEnd('\') -and $registration.size_kib -gt 0 -and $registration.quiet) 'InstallLocation, EstimatedSize and quiet-uninstall metadata exist'
    Check-Manifest
    Check (-not (Test-Path (Join-Path $installDir 'extras/VBCABLE_Driver_Pack45.zip'))) 'Optional virtual-cable driver is not bundled'
    Check (-not (Test-Path (Join-Path $installDir 'gui/opengl32sw.dll')) -and -not (Test-Path (Join-Path $installDir 'gui/D3Dcompiler_47.dll'))) 'Optional software-OpenGL/D3D runtime is not bundled'
    Check ((Get-FileHash (Join-Path $installDir 'licenses/qt/source/qtbase-everywhere-src-6.8.3.tar.xz')).Hash -eq '56001B905601BB9023D399F3BA780D7FA940F3E4861E496A7C490331F49E0B80' -and
        (Get-FileHash (Join-Path $installDir 'licenses/qt/source/qtsvg-everywhere-src-6.8.3.tar.xz')).Hash -eq '35EB516460F00F264EB504BAA253432384351CF23FB9980A5857190E8DEEF438') 'Complete matching Qt source archives accompany installed libraries'
    $shell = New-Object -ComObject WScript.Shell
    Check ((Get-FileHash (Join-Path $installDir 'licenses/hamlib/source/hamlib-4.7.2.tar.gz')).Hash -eq 'AE1FCF2DBC80EA0786EA8F047B09399C3F7737D1930442F61A031708ED33E88F' -and
        (Get-FileHash (Join-Path $installDir 'licenses/libusb/source/libusb-1.0.30.tar.bz2')).Hash -eq 'FEA36F34F9156400209595E300840767AB1A385EDE1DC7EE893015AEA9C6DBAF') 'Complete matching Hamlib/libusb sources accompany installed CAT libraries'
    Check ((Get-FileHash (Join-Path $installDir 'gui/libusb-1.0.dll')).Hash -eq '9858E2381221619E26A78A2824E5970C2A7EC48CDB75071C19EB7B61BAA268E2' -and
        (Test-Path (Join-Path $installDir 'gui/libhamlib-4.dll'))) 'Hamlib and updated libusb runtimes installed locally'
    Check ((Test-Path (Join-Path $installDir 'licenses/REPLACING_CAT.html')) -and
        (Test-Path (Join-Path $installDir 'docs/RADIO_CONTROL.html'))) 'Offline CAT safety and library replacement guides installed'
    $testShortcuts = @((Join-Path $startMenu 'FEC-RTTY - M0NXD.lnk'))
    if (-not $IsolatedShortcuts) { $testShortcuts += $desktopShortcut }
    foreach ($path in $testShortcuts) {
        Check (Test-Path $path) "Shortcut created: $([IO.Path]::GetFileName($path))"
        $link = $shell.CreateShortcut($path)
        Check ($link.TargetPath -eq (Join-Path $installDir 'gui/fectty-gui.exe') -and $link.WorkingDirectory -eq (Join-Path $installDir 'gui')) 'Shortcut points to installed GUI with correct runtime working directory'
    }
    Check (Test-Path (Join-Path $startMenu 'Uninstall FEC-RTTY.lnk')) 'Start-menu uninstall shortcut exists'
    foreach ($relative in @('gui/fectty-gui.exe','bin/fectty-live.exe')) {
        $probeArgs = if ($relative.StartsWith('gui')) { @('--help') } else { @('--list') }
        $probe = Run-Program (Join-Path $installDir $relative) $probeArgs (($relative -replace '[/\\.]','-')+'-probe')
        Check ($probe.exit -eq 0) "Installed $relative starts with Windows-only PATH and no Qt environment overrides"
    }
    foreach ($exe in @('fectty-tests.exe','fectty-audit-tests.exe','fectty-cat-tests.exe','fectty-gui-text-tests.exe','fectty-waterfall-tests.exe')) {
        $test = Run-Program (Join-Path $installDir "gui/$exe") @() ($exe -replace '\.exe$','') 120
        Check ($test.exit -eq 0) "Installed regression passes: $exe"
    }
    # Two simultaneous visible installed windows; no audio/settings writes.
    $windows = [Collections.Generic.List[Diagnostics.Process]]::new()
    try {
        foreach ($label in @('a','b')) {
            $info = [Diagnostics.ProcessStartInfo]::new()
            $info.FileName = Join-Path $installDir 'gui/fectty-gui.exe'
            foreach ($argument in @('--run-seconds','5','--report',(Join-Path $run "idle-$label.json"))) { $info.ArgumentList.Add($argument) }
            $info.UseShellExecute = $false
            $info.WindowStyle = [Diagnostics.ProcessWindowStyle]::Normal
            $info.WorkingDirectory = Join-Path $installDir 'gui'
            $info.Environment['PATH'] = (Join-Path $env:WINDIR 'System32')+';'+$env:WINDIR
            foreach ($variable in @('QT_PLUGIN_PATH','QT_QPA_PLATFORM_PLUGIN_PATH','QT_QPA_PLATFORM','QML2_IMPORT_PATH','QML_IMPORT_PATH')) { $null = $info.Environment.Remove($variable) }
            $windows.Add([Diagnostics.Process]::Start($info))
        }
        $deadline = [DateTime]::UtcNow.AddSeconds(3)
        do {
            Start-Sleep -Milliseconds 100
            foreach ($window in $windows) { $window.Refresh() }
            $ready = @($windows | Where-Object { -not $_.HasExited -and $_.MainWindowTitle -eq 'FEC-RTTY - M0NXD' -and $_.Responding }).Count -eq 2
        } while (-not $ready -and [DateTime]::UtcNow -lt $deadline)
        Check $ready 'Two simultaneous visible installed GUI windows are responding with correct title'
        foreach ($window in $windows) {
            if (-not $window.WaitForExit(15000)) { throw 'Visible GUI did not close after its bench timer.' }
            Check ($window.ExitCode -eq 0) 'Simultaneous visible GUI closes cleanly'
        }
    } finally {
        foreach ($window in $windows) {
            if (-not $window.HasExited) { $window.Kill($true); $window.WaitForExit() }
            $window.Dispose()
        }
    }
    if ($TestAudio) {
        $guiExe = Join-Path $installDir 'gui/fectty-gui.exe'
        $winmmList = Run-Program (Join-Path $installDir 'bin/fectty-live.exe') @('--list') 'winmm-list'
        $paList = Run-Program (Join-Path $installDir 'gui/fectty-portaudio-smoke.exe') @('--list') 'portaudio-list'
        $wi=[regex]::Match($winmmList.output,'(?m)^\s*([0-9]+): CABLE Output')
        $wo=[regex]::Match($winmmList.output,'(?m)^\s*([0-9]+): CABLE Input')
        $pi=[regex]::Match($paList.output,'portaudio:([0-9]+): CABLE Output[^\r\n]*\[MME\]')
        $po=[regex]::Match($paList.output,'portaudio:([0-9]+): CABLE Input[^\r\n]*\[MME\]')
        Check ($wi.Success -and $wo.Success) 'Current WinMM cable indices/names verified before transmission'
        Check ($pi.Success -and $po.Success) 'Current PortAudio MME cable indices/names verified before transmission'
        foreach ($backend in @('winmm','portaudio')) {
            $inputIndex = if ($backend -eq 'winmm') { $wi.Groups[1].Value } else { $pi.Groups[1].Value }
            $outputIndex = if ($backend -eq 'winmm') { $wo.Groups[1].Value } else { $po.Groups[1].Value }
            $audio = Run-Program (Join-Path $PSHOME 'pwsh.exe') @('-NoProfile','-File',(Join-Path $project 'tools/test-gui-direct.ps1'),'-GuiExe',$guiExe,'-Backend',$backend,'-InputDevice',$inputIndex,'-OutputDevice',$outputIndex,'-SplitOffsets','-CatDummy','-EvidenceDir',(Join-Path $run "audio-$backend")) "direct-$backend" 90 -Visible
            Check ($audio.exit -eq 0) "Installed visible two-direction exact-text audio + Hamlib Dummy CAT test: $backend"
        }
    }
    $userFile = Join-Path $installDir 'user-created.txt'
    [IO.File]::WriteAllText($userFile,'Keep this user-created file.',$utf8)
    $repairFile = Join-Path $installDir 'gui/Qt6Widgets.dll'
    # This is our isolated test copy's installer-owned file, not the project/runtime.
    if (-not $repairFile.StartsWith($installDir+[IO.Path]::DirectorySeparatorChar)) { throw 'Repair-test target outside isolated install.' }
    Remove-Item -LiteralPath $repairFile
    $repaired = Run-Program $SetupExe $arguments 'repair' 120
    Check ($repaired.exit -eq 0) 'Rerun/repair succeeds with the same stable application identity'
    Check-Manifest
    Check ((Get-Content $userFile -Raw) -eq 'Keep this user-created file.') 'Reinstall preserves user-created files'
    $uninstallRegistration = Read-Installation
    Uninstall-TestCopy
    Check (-not (Test-Path (Join-Path $installDir 'gui/fectty-gui.exe')) -and -not (Test-Path (Join-Path $installDir 'gui/Qt6Widgets.dll'))) 'Owned application/runtime files removed'
    Check (-not (Test-Path (Join-Path $startMenu 'FEC-RTTY - M0NXD.lnk')) -and ($IsolatedShortcuts -or -not (Test-Path $desktopShortcut))) 'Installed test shortcuts removed'
    if ($IsolatedShortcuts) {
        $shortcutsUnchanged = $true
        foreach ($file in $preservedShortcuts) {
            if (-not (Test-Path -LiteralPath $file.path) -or (Get-FileHash -LiteralPath $file.path).Hash -ne $file.sha256) { $shortcutsUnchanged = $false }
        }
        Check $shortcutsUnchanged 'Pre-existing Start-menu/desktop shortcuts unchanged'
    }
    Check ((Get-Content $userFile -Raw) -eq 'Keep this user-created file.') 'Uninstall preserves user-created files'
    $settingsAfter = if (Test-Path $settings) { (Get-FileHash $settings).Hash } else { $null }
    Check ($settingsBefore -eq $settingsAfter) 'Existing operator settings unchanged through install/test/repair/uninstall'
    $driversAfter = @(Get-CimInstance Win32_PnPEntity | Where-Object Name -match 'VB-Audio|VB-CABLE|VBCABLE' | ForEach-Object { $_.DeviceID+'|'+$_.Status } | Sort-Object)
    Check (-not (Compare-Object $driversBefore $driversAfter)) 'Existing VB-Audio devices unchanged'
} catch {
    $failure = $_.Exception.Message
    Write-Warning $failure
    # Cleanup only if the registered path is the exact isolated test copy.
    try { Uninstall-TestCopy } catch { Write-Warning "Test cleanup needs attention: $($_.Exception.Message)" }
} finally {
    # /GROUP must be honored, but also recover byte-identical original shortcuts
    # if an older/broken installer disregards isolation or fails mid-test.
    foreach ($file in $preservedShortcuts) {
        if (-not (Test-Path -LiteralPath $file.path) -or (Get-FileHash -LiteralPath $file.path).Hash -ne $file.sha256) {
            if ((Get-FileHash -LiteralPath $file.backup).Hash -ne $file.sha256) { throw 'Original shortcut backup corrupted; manual recovery required.' }
            New-Item -ItemType Directory -Path (Split-Path $file.path) -Force | Out-Null
            Copy-Item -LiteralPath $file.backup -Destination $file.path -Force
            Write-Warning "Restored original shortcut: $([IO.Path]::GetFileName($file.path))"
        }
    }
    $report = [ordered]@{ installer=$SetupExe; sha256=(Get-FileHash $SetupExe).Hash; run_directory=$run; isolated_install=$installDir; tests=$results; failure=$failure; audio_test=[bool]$TestAudio; isolated_shortcuts=[bool]$IsolatedShortcuts; preserved_shortcuts=$preservedShortcuts; settings_before=$settingsBefore; registration_after=(Read-Installation); note='Host acceptance, not a clean VM or driver installation test. Isolated-shortcut mode uses a separate Start-menu group and does not test desktop-shortcut creation. Synthetic user files/test artifacts retained.' }
    [IO.File]::WriteAllText((Join-Path $run 'acceptance.json'),($report | ConvertTo-Json -Depth 7),$utf8)
    $publicReport = ConvertTo-PublicPaths ($report | ConvertTo-Json -Depth 7) -ProjectRoot $project
    [IO.File]::WriteAllText((Join-Path $run 'acceptance.public.json'),$publicReport,$utf8)
    Write-Output "Installer acceptance report: $(Join-Path $run 'acceptance.json')"
}
if ($failure) { throw $failure }
