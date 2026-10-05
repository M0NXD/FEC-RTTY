#requires -Version 7.0
[CmdletBinding()]
param(
    [string]$ReleaseDir,
    [string]$AcceptedArchive,
    [string]$IsccExe,
    [string]$QtRoot,
    [string]$RuntimeLicenseRoot = 'C:\msys64\mingw64\share',
    [string]$QtLicenseDir,
    [string]$QtSourceDir,
    [string]$OutputDir
)
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$project = Split-Path -Parent $PSScriptRoot
if (-not $ReleaseDir) { $ReleaseDir = Join-Path $project 'releases/fectty-v0.46.0-cat-20261005' }
if (-not $AcceptedArchive) { $AcceptedArchive = Join-Path $project 'releases/fectty-v0.46.0-cat-20261005.zip' }
if (-not $QtRoot) { $QtRoot = Join-Path $project 'third_party/qt/6.8.3/mingw_64' }
if (-not $QtLicenseDir) { $QtLicenseDir = Join-Path $project 'third_party/installer-licenses' }
if (-not $QtSourceDir) { $QtSourceDir = Join-Path $project 'third_party/public-qt-sources' }
if (-not $OutputDir) { $OutputDir = Join-Path $project 'releases/v0.46.0-public' }
if (-not $IsccExe) {
    $candidates = @((Join-Path $project 'third_party/inno-setup-6.7.3/ISCC.exe'),
        'C:\Program Files (x86)\Inno Setup 6\ISCC.exe', 'C:\Program Files\Inno Setup 6\ISCC.exe')
    $IsccExe = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
    if (-not $IsccExe) { throw 'Install a compatible Inno Setup compiler or pass -IsccExe. No downloads are made by this script.' }
}
$ReleaseDir = (Resolve-Path -LiteralPath $ReleaseDir).Path
$IsccExe = (Resolve-Path -LiteralPath $IsccExe).Path
$QtSourceDir = (Resolve-Path -LiteralPath $QtSourceDir).Path
$cmake = Get-Content (Join-Path $project 'source/CMakeLists.txt') -Raw
$versionMatch = [regex]::Match($cmake, 'project\(fectty VERSION ([0-9]+\.[0-9]+\.[0-9]+)')
if (-not $versionMatch.Success) { throw 'Application version is missing.' }
$version = $versionMatch.Groups[1].Value
$contract = Get-Content (Join-Path $PSScriptRoot 'accepted-release.json') -Raw | ConvertFrom-Json
if ($version -ne $contract.version) { throw 'Version differs from the recorded accepted runtime contract; rebuild and retest.' }
$gui = Join-Path $ReleaseDir 'gui/fectty-gui.exe'
$live = Join-Path $ReleaseDir 'bin/fectty-live.exe'
if ((Get-FileHash $gui).Hash -ne $contract.gui_sha256 -or (Get-FileHash $live).Hash -ne $contract.live_sha256) {
    throw 'Runtime binaries differ from the accepted CAT release.'
}
$hamlibRoot = Join-Path $project 'third_party/hamlib-4.7.2'
$libusbRoot = Join-Path $project 'third_party/libusb-1.0.30'
$catSources = @(
    @{ root=$hamlibRoot; name='hamlib-4.7.2.tar.gz'; hash='AE1FCF2DBC80EA0786EA8F047B09399C3F7737D1930442F61A031708ED33E88F'; folder='hamlib' },
    @{ root=$libusbRoot; name='libusb-1.0.30.tar.bz2'; hash='FEA36F34F9156400209595E300840767AB1A385EDE1DC7EE893015AEA9C6DBAF'; folder='libusb' }
)
foreach ($input in $catSources) {
    if ((Get-FileHash (Join-Path $input.root $input.name)).Hash -ne $input.hash) { throw "CAT source checksum mismatch: $($input.name)" }
}
$qtSources = @{
    'qtbase-everywhere-src-6.8.3.tar.xz' = '56001B905601BB9023D399F3BA780D7FA940F3E4861E496A7C490331F49E0B80'
    'qtsvg-everywhere-src-6.8.3.tar.xz' = '35EB516460F00F264EB504BAA253432384351CF23FB9980A5857190E8DEEF438'
}
foreach ($name in $qtSources.Keys) {
    if ((Get-FileHash (Join-Path $QtSourceDir $name)).Hash -ne $qtSources[$name]) { throw "Official Qt source checksum mismatch: $name" }
}

# Verify every runtime/plugin, not just the two known executable hashes.
$AcceptedArchive = (Resolve-Path -LiteralPath $AcceptedArchive).Path
if ((Get-FileHash $AcceptedArchive).Hash -ne $contract.archive_sha256) { throw 'Accepted release archive differs from its pinned checksum.' }
$archive = [IO.Compression.ZipFile]::OpenRead($AcceptedArchive)
try {
    $prefix = $contract.archive_prefix
    $runtimeEntries = @($archive.Entries | Where-Object { $_.FullName -match ('^'+[regex]::Escape($prefix)+'(gui|bin)/') -and -not $_.FullName.EndsWith('/') })
    $runtimeFiles = @(Get-ChildItem (Join-Path $ReleaseDir 'gui'),(Join-Path $ReleaseDir 'bin') -File -Recurse)
    if ($runtimeEntries.Count -eq 0 -or $runtimeEntries.Count -ne $runtimeFiles.Count) { throw 'Runtime folder/archive file inventory mismatch.' }
    foreach ($entry in $runtimeEntries) {
        $relative = $entry.FullName.Substring($prefix.Length)
        $path = Join-Path $ReleaseDir $relative
        if (-not (Test-Path -LiteralPath $path)) { throw "Runtime file missing: $relative" }
        $stream = $entry.Open()
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $expected = [BitConverter]::ToString($sha.ComputeHash($stream)).Replace('-','') }
        finally { $stream.Dispose(); $sha.Dispose() }
        if ((Get-FileHash $path).Hash -ne $expected) { throw "Runtime file differs from accepted archive: $relative" }
    }
    Write-Output "Verified accepted archive and all $($runtimeEntries.Count) runtime/plugin files."
} finally { $archive.Dispose() }

# Refuse to ship edited modem code beside a stale accepted executable.
foreach ($folder in @('src','include','tests')) {
    $sourceRoot = Join-Path $project "source/$folder"
    foreach ($file in Get-ChildItem $sourceRoot -File -Recurse) {
        $relative = [IO.Path]::GetRelativePath((Join-Path $project 'source'), $file.FullName)
        $releaseSource = Join-Path $ReleaseDir "source/$relative"
        if (-not (Test-Path -LiteralPath $releaseSource) -or (Get-FileHash $file.FullName).Hash -ne (Get-FileHash $releaseSource).Hash) {
            throw "Source/runtime mismatch: $relative; rebuild and record a new release before packaging."
        }
    }
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss-fff'
$stage = Join-Path $PSScriptRoot "build/$stamp"
$payload = Join-Path $stage 'payload'
if (Test-Path -LiteralPath $stage) { throw 'Staging directory collision.' }
New-Item -ItemType Directory -Path $payload -Force | Out-Null
function Copy-PayloadFile([string]$Source, [string]$Relative) {
    $destination = Join-Path $payload $Relative
    New-Item -ItemType Directory -Path (Split-Path -Parent $destination) -Force | Out-Null
    Copy-Item -LiteralPath $Source -Destination $destination
}
foreach ($folder in @('gui','bin')) {
    foreach ($file in Get-ChildItem (Join-Path $ReleaseDir $folder) -File -Recurse) {
        # This Widgets application uses raster painting, not the optional
        # software OpenGL fallback or Microsoft's D3D compiler redistribution.
        if ($file.Name -in @('opengl32sw.dll','D3Dcompiler_47.dll')) { continue }
        Copy-PayloadFile $file.FullName ([IO.Path]::GetRelativePath($ReleaseDir,$file.FullName))
    }
}
$tracked = & git -C $project ls-files -- source docs tools evidence installer
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate canonical source/documentation.' }
# Include pending installer/docs edits while creating the first installer.
$pending = & git -C $project ls-files --others --exclude-standard -- source docs tools evidence installer
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate current installer/documentation files.' }
foreach ($relative in @($tracked) + @($pending) | Sort-Object -Unique) {
    if ($relative -match '\.(wav|dll|zip|exe)$' -or $relative -match '(^|/)build[^/]*/') { continue }
    Copy-PayloadFile (Join-Path $project $relative) $relative
}
foreach ($name in @('README.md','PROJECT_INDEX.md')) { Copy-PayloadFile (Join-Path $project $name) $name }
foreach ($name in $qtSources.Keys) { Copy-PayloadFile (Join-Path $QtSourceDir $name) "licenses/qt/source/$name" }
foreach ($input in $catSources) { Copy-PayloadFile (Join-Path $input.root $input.name) "licenses/$($input.folder)/source/$($input.name)" }
foreach ($name in @('COPYING.LIB.txt','COPYING.txt','LICENSE.txt','AUTHORS.txt','README.w64-bin.txt')) {
    Copy-PayloadFile (Join-Path $hamlibRoot "sdk/hamlib-w64-4.7.2/$name") "licenses/hamlib/$name"
}
Copy-PayloadFile (Join-Path $libusbRoot 'libusb-1.0.30/COPYING') 'licenses/libusb/COPYING'
Copy-PayloadFile (Join-Path $libusbRoot 'PKGBUILD') 'licenses/libusb/PKGBUILD'
Copy-PayloadFile (Join-Path $PSScriptRoot 'resources/REPLACING_CAT.md') 'licenses/REPLACING_CAT.md'
Copy-PayloadFile (Join-Path $PSScriptRoot 'resources/REPLACING_QT.md') 'licenses/qt/REPLACING_QT.md'
Copy-PayloadFile (Join-Path $PSScriptRoot 'resources/START_HERE.html') 'index.html'
Copy-PayloadFile (Join-Path $PSScriptRoot 'THIRD_PARTY_NOTICES.md') 'licenses/THIRD_PARTY_NOTICES.md'
foreach ($file in Get-ChildItem $QtLicenseDir -File) { Copy-PayloadFile $file.FullName "licenses/qt/$($file.Name)" }
foreach ($required in @('LGPL-3.0-only.txt','GPL-3.0-only.txt')) {
    if (-not (Test-Path (Join-Path $payload "licenses/qt/$required"))) { throw "Required Qt license missing: $required" }
}
foreach ($file in Get-ChildItem (Join-Path $QtRoot 'sbom') -File) { Copy-PayloadFile $file.FullName "licenses/qt/sbom/$($file.Name)" }
Copy-PayloadFile (Join-Path $RuntimeLicenseRoot 'doc/portaudio/LICENSE.txt') 'licenses/portaudio/LICENSE.txt'
foreach ($file in Get-ChildItem (Join-Path $RuntimeLicenseRoot 'licenses/gcc-libs') -File) { Copy-PayloadFile $file.FullName "licenses/gcc/$($file.Name)" }
foreach ($file in Get-ChildItem (Join-Path $RuntimeLicenseRoot 'licenses/libwinpthread') -File) { Copy-PayloadFile $file.FullName "licenses/winpthreads/$($file.Name)" }
Copy-PayloadFile (Join-Path (Split-Path -Parent $IsccExe) 'License.txt') 'licenses/inno/License.txt'

# Render offline Markdown with PowerShell's maintained parser, then align IDs
# with the GitHub-style anchors used by the canonical documents.
$utf8 = [Text.UTF8Encoding]::new($false)
$css = 'body{font:1rem/1.65 system-ui,Segoe UI,sans-serif;background:#111923;color:#e7edf5;max-width:72rem;margin:auto;padding:2rem}a{color:#79c8ff}h1,h2,h3{line-height:1.3;color:#7ed7c3}pre,code{background:#172331}pre{padding:1rem;overflow:auto}table{border-collapse:collapse;width:100%;display:block;overflow:auto}th,td{border:1px solid #40566d;padding:.6rem;text-align:left}img{max-width:100%;height:auto}code{overflow-wrap:anywhere}nav{margin-bottom:2rem}'
function Convert-OfflineLinks([string]$Html) {
    [regex]::Replace($Html, 'href="([^"?#]+)\.md(?=["?#])', [Text.RegularExpressions.MatchEvaluator]{ param($match)
        # External Markdown pages remain external; only local pages are rendered.
        if ($match.Groups[1].Value -match '^(?:[a-zA-Z][a-zA-Z0-9+.-]*:|//)') { return $match.Value }
        return 'href="'+$match.Groups[1].Value+'.html'
    })
}
$linkProbe = '<a href="guide.md#section"></a><a href="https://example.org/guide.md"></a><a href="//example.org/guide.md"></a>'
$linkExpected = '<a href="guide.html#section"></a><a href="https://example.org/guide.md"></a><a href="//example.org/guide.md"></a>'
if ((Convert-OfflineLinks $linkProbe) -cne $linkExpected) { throw 'Offline/external link conversion regression.' }
foreach ($markdown in Get-ChildItem $payload -Filter '*.md' -Recurse -File) {
    $rendered = (ConvertFrom-Markdown -LiteralPath $markdown.FullName).Html
    $anchorCounts = @{}
    $rendered = [regex]::Replace($rendered, '<h([1-6])[^>]*>(.*?)</h\1>', [Text.RegularExpressions.MatchEvaluator]{ param($match)
        $plain = [Net.WebUtility]::HtmlDecode([regex]::Replace($match.Groups[2].Value, '<[^>]+>', ''))
        $slug = [regex]::Replace($plain.Trim().ToLowerInvariant(), '[^\p{L}\p{Nd}_\- ]', '') -replace ' ', '-'
        if ($anchorCounts.ContainsKey($slug)) { $anchorCounts[$slug]++; $slug += '-'+$anchorCounts[$slug] } else { $anchorCounts[$slug] = 0 }
        '<h'+$match.Groups[1].Value+' id="'+[Net.WebUtility]::HtmlEncode($slug)+'">'+$match.Groups[2].Value+'</h'+$match.Groups[1].Value+'>'
    })
    $rendered = Convert-OfflineLinks $rendered
    $relative = [IO.Path]::GetRelativePath($payload, $markdown.FullName)
    $prefix = '../' * (($relative -split '[\\/]').Count - 1)
    $title = [Net.WebUtility]::HtmlEncode($markdown.BaseName)
    $html = '<!doctype html><html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1"><title>FEC-RTTY | '+$title+'</title><style>'+$css+'</style></head><body><nav><a href="'+$prefix+'index.html">FEC-RTTY: getting started</a></nav>'+$rendered+'</body></html>'
    [IO.File]::WriteAllText([IO.Path]::ChangeExtension($markdown.FullName,'.html'), $html, $utf8)
}
# Validate primary offline navigation and GitHub-compatible section anchors.
$offlineLinks = 0
$primaryPages = @('index.html','README.html','docs/PROJECT_GUIDE.html','docs/PROTOCOL.html','docs/GUI_PACKAGE.html','docs/RADIO_CONTROL.html','docs/INSTALLER.html','docs/DEVELOPMENT_PLAN.html','licenses/THIRD_PARTY_NOTICES.html')
foreach ($relative in $primaryPages) {
    $page = Join-Path $payload $relative
    $body = Get-Content $page -Raw -Encoding UTF8
    foreach ($match in [regex]::Matches($body,'(?:href|src)="([^"]+)"')) {
        $url = [Net.WebUtility]::HtmlDecode($match.Groups[1].Value)
        if ($url -match '^[a-zA-Z][a-zA-Z0-9+.-]*:') { continue }
        $parts = $url -split '#',2
        $target = if ($parts[0]) { [IO.Path]::GetFullPath((Join-Path (Split-Path $page) ([Uri]::UnescapeDataString($parts[0])))) } else { $page }
        $offlineLinks++
        if (-not $target.StartsWith($payload+[IO.Path]::DirectorySeparatorChar,[StringComparison]::OrdinalIgnoreCase) -or -not (Test-Path -LiteralPath $target)) { throw "Offline link missing or outside payload: $relative -> $url" }
        if ($parts.Count -eq 2 -and $parts[1] -and (Get-Content $target -Raw -Encoding UTF8) -notmatch ('id="'+[regex]::Escape($parts[1])+'"')) { throw "Offline section anchor missing: $relative -> $url" }
    }
}
Write-Output "Verified $offlineLinks local links/anchors across $($primaryPages.Count) primary offline pages."
$files = @(Get-ChildItem $payload -File -Recurse | Sort-Object FullName | ForEach-Object {
    [pscustomobject][ordered]@{ path = [IO.Path]::GetRelativePath($payload,$_.FullName).Replace('\','/'); bytes = $_.Length; sha256 = (Get-FileHash $_.FullName).Hash }
})
$commit = & git -C $project rev-parse HEAD
$stagedTree = & git -C $project write-tree
$sourceTree = & git -C $project rev-parse "${stagedTree}:source"
$dirty = @(& git -C $project status --porcelain).Count -gt 0
$manifest = [ordered]@{ app_id='FEC-RTTY-M0NXD'; app_version=$version; installer_revision=1; built_at=(Get-Date).ToString('o'); source_tree_sha1=$sourceTree; source_working_tree_dirty=$dirty; inno_compiler_file_version=(Get-Item $IsccExe).VersionInfo.FileVersion; inno_compiler_sha256=(Get-FileHash $IsccExe).Hash; driver_policy='No bundled driver; separate official vendor download only'; files=$files }
$manifestPath = Join-Path $payload 'installation-manifest.json'
[IO.File]::WriteAllText($manifestPath, ($manifest | ConvertTo-Json -Depth 6), $utf8)
New-Item -ItemType Directory -Path $OutputDir -Force | Out-Null
$OutputDir = (Resolve-Path $OutputDir).Path
& $IsccExe '/Q' "/DPayloadDir=$payload" "/DOutputPath=$OutputDir" "/DAppVersion=$version" (Join-Path $PSScriptRoot 'fec-rtty.iss')
if ($LASTEXITCODE -ne 0) { throw "Inno Setup compilation failed: $LASTEXITCODE" }
$setup = Join-Path $OutputDir "FEC-RTTY-$version-Setup-x64.exe"
$hash = (Get-FileHash $setup).Hash
[IO.File]::WriteAllText("$setup.sha256", "$hash  $([IO.Path]::GetFileName($setup))`n", $utf8)
Copy-Item -LiteralPath $manifestPath -Destination "$setup.manifest.json"
$portable = Join-Path $OutputDir "FEC-RTTY-$version-Windows-x64.zip"
if (Test-Path -LiteralPath $portable) { throw 'Portable output already exists; choose an empty output folder.' }
[IO.Compression.ZipFile]::CreateFromDirectory($payload,$portable,[IO.Compression.CompressionLevel]::Optimal,$false)
$portableHash = (Get-FileHash $portable).Hash
[IO.File]::WriteAllText("$portable.sha256", "$portableHash  $([IO.Path]::GetFileName($portable))`n", $utf8)
Write-Output "Installer: $setup"
Write-Output "SHA256: $hash"
Write-Output "Payload: $($files.Count) files, $([Math]::Round(($files | Measure-Object bytes -Sum).Sum / 1MB,2)) MiB"
Write-Output "Staging retained: $stage"
