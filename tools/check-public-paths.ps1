#requires -Version 7.0
[CmdletBinding()]
param([switch]$History, [switch]$SelfTest)
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'public-paths.ps1')
$projectRoot = Split-Path -Parent $PSScriptRoot

if ($SelfTest) {
    $testProfile = ([char]67) + ':\' + 'Users\PRIVATE_OPERATOR'
    $testProject = $testProfile + '\Documents\ChatGPT\FEC-RTTY'
    $cases = @(
        @(($testProject + '\source'), '<project-root>\source'),
        @(($testProject.Replace('\','\\') + '\\source'), '<project-root>\\source'),
        @(($testProject.Replace('\','/') + '/source'), '<project-root>/source'),
        @(($testProfile + '\AppData\Local\FEC-RTTY'), '%LOCALAPPDATA%\FEC-RTTY'),
        @(($testProfile + '\AppData\Roaming\Microsoft'), '%APPDATA%\Microsoft'),
        @(($testProfile + '\OneDrive\Desktop'), '%USERPROFILE%\OneDrive\Desktop'),
        @(($testProfile + '\.codex\.chatgpt-projects\private-id\artifacts\test.zip'), 'releases/test.zip')
    )
    foreach ($case in $cases) {
        if (-not (Test-PrivatePath $case[0]) -or (ConvertTo-PublicPaths $case[0]) -cne $case[1]) { throw 'Path redaction self-test failed.' }
    }
    foreach ($safe in @('https://github.com/M0NXD/FEC-RTTY', 'releases/example/gui/program.exe', '%LOCALAPPDATA%\FEC-RTTY', '<project-root>/source')) {
        if (Test-PrivatePath $safe) { throw 'Safe public reference incorrectly rejected.' }
    }
    Write-Output "PASS: $($cases.Count) redaction cases and four safe-reference cases."
}

$files = @(& git -C $projectRoot ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate tracked project files.' }
$failures = [Collections.Generic.List[string]]::new()
foreach ($file in $files) {
    # Include binary metadata rather than excluding images from the scan.
    $text = [Text.Encoding]::UTF8.GetString([IO.File]::ReadAllBytes((Join-Path $projectRoot $file)))
    if (Test-PrivatePath $text) { $failures.Add($file) }
}
if ($History) {
    $commits = @(& git -C $projectRoot rev-list main)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate published branch history.' }
    # Git prints only commit/path identities, never the matched private value.
    foreach ($commit in $commits) {
        $paths = @(& git -C $projectRoot grep -I -l -i -E '([a-z]:[\\/]+Users[\\/]+[^\\/[:space:]]+|/(home|Users)/[^/[:space:]]+)' $commit -- .)
        if ($LASTEXITCODE -gt 1) { throw 'Cannot scan Git history.' }
        foreach ($path in $paths) { $failures.Add($path) }
    }
}
if ($failures.Count) {
    $failures | Sort-Object -Unique | Write-Output
    throw "Private filesystem paths found in $($failures.Count) file/revision references. Redact before publishing."
}
Write-Output "PASS: $($files.Count) tracked files have no private user-profile paths."
if ($History) { Write-Output "PASS: all $($commits.Count) main-branch revisions checked." }
