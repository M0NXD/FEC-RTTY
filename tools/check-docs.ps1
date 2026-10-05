#requires -Version 7.0
[CmdletBinding()]
param()
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$projectRoot = Split-Path -Parent $PSScriptRoot
$tracked = @(& git -C $projectRoot ls-files)
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate tracked files.' }
$pending = @(& git -C $projectRoot ls-files --others --exclude-standard)
if ($LASTEXITCODE -ne 0) { throw 'Cannot enumerate new publication files.' }
$publicFiles = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
foreach ($path in @($tracked) + @($pending)) { $null = $publicFiles.Add($path.Replace('\','/')) }
$pages = @($publicFiles | Where-Object { $_ -like '*.md' } | Sort-Object)
$anchors = @{}
$renderedPages = @{}
foreach ($relative in $pages) {
    $markdown = Get-Content -LiteralPath (Join-Path $projectRoot $relative) -Raw -Encoding UTF8
    if ($relative -like 'releases/*' -and $relative -ne 'releases/README.md' -and $markdown -notmatch '> Historical milestone:') {
        throw "Archive page lacks a historical/current-instructions notice: $relative"
    }
    if (($relative -like 'docs/TEST_RESULTS_*.md' -or ($relative -like 'evidence/*.md' -and $relative -ne 'evidence/README.md')) -and $markdown -notmatch '> Dated development/test record\.') {
        throw "Dated record lacks a scope notice: $relative"
    }
    $html = (ConvertFrom-Markdown -LiteralPath (Join-Path $projectRoot $relative)).Html
    $renderedPages[$relative] = $html
    $ids = [Collections.Generic.HashSet[string]]::new([StringComparer]::Ordinal)
    $counts = @{}
    foreach ($match in [regex]::Matches($html, '<h[1-6][^>]*>(.*?)</h[1-6]>', 'Singleline')) {
        $plain = [Net.WebUtility]::HtmlDecode([regex]::Replace($match.Groups[1].Value, '<[^>]+>', ''))
        $slug = [regex]::Replace($plain.Trim().ToLowerInvariant(), '[^\p{L}\p{Nd}_\- ]', '') -replace ' ', '-'
        if ($counts.ContainsKey($slug)) { $counts[$slug]++; $slug += '-'+$counts[$slug] } else { $counts[$slug] = 0 }
        $null = $ids.Add($slug)
    }
    foreach ($match in [regex]::Matches($html, '<a[^>]+(?:id|name)="([^"]+)"')) { $null = $ids.Add($match.Groups[1].Value) }
    $anchors[$relative] = $ids
}
$broken = [Collections.Generic.List[object]]::new()
$checkedLinks = 0
foreach ($relative in $pages) {
    foreach ($match in [regex]::Matches($renderedPages[$relative], '(?:href|src)="([^"]+)"')) {
        $url = [Net.WebUtility]::HtmlDecode($match.Groups[1].Value)
        if ($url -match '^[a-zA-Z][a-zA-Z0-9+.-]*:' -or $url.StartsWith('//')) { continue }
        $checkedLinks++
        $parts = $url -split '#',2
        $pathPart = [Uri]::UnescapeDataString(($parts[0] -split '\?',2)[0])
        $target = $relative
        if ($pathPart) {
            $absolute = [IO.Path]::GetFullPath((Join-Path (Split-Path (Join-Path $projectRoot $relative)) $pathPart))
            $target = [IO.Path]::GetRelativePath($projectRoot, $absolute).Replace('\','/')
        }
        if (-not $publicFiles.Contains($target)) {
            $broken.Add([pscustomobject]@{page=$relative; link=$url; reason='Not in public file inventory'})
        } elseif ($parts.Count -eq 2 -and $parts[1] -and $anchors.ContainsKey($target)) {
            $anchor = [Uri]::UnescapeDataString($parts[1])
            if (-not $anchors[$target].Contains($anchor)) { $broken.Add([pscustomobject]@{page=$relative; link=$url; reason='Missing GitHub-style heading anchor'}) }
        }
    }
}
if ($broken.Count) {
    $broken | Format-Table -AutoSize | Out-String -Width 240 | Write-Output
    throw "$($broken.Count) broken documentation links/anchors."
}
Write-Output "PASS: $($pages.Count) Markdown pages render; $checkedLinks local links/anchors resolve in the public inventory."
