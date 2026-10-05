# Shared helpers for publishing evidence without workstation-specific paths.
Set-StrictMode -Version Latest

function ConvertTo-PublicPaths {
    param([AllowEmptyString()][string]$Text, [string]$ProjectRoot = (Split-Path -Parent $PSScriptRoot))
    # Match both ordinary Windows paths and JSON-escaped backslashes. Keep the
    # suffix so test filenames and relative locations remain useful.
    $separator = '[\\/]+'
    $userPrefix = '(?i)(?<![a-z0-9])[a-z]:' + $separator + 'Users' + $separator + '[^\\/\s"<>]+'
    $workspace = $userPrefix + $separator + 'Documents' + $separator + 'ChatGPT' + $separator + 'FEC-(?:R)?TTY'
    $Text = [regex]::Replace($Text, $workspace, '<project-root>')
    # Historical conversation exports need a filename, not an internal app ID.
    $artifact = $userPrefix + $separator + '\.codex' + $separator + '\.chatgpt-projects' + $separator + '[^\\/\s"<>]+' + $separator + 'artifacts' + $separator
    $Text = [regex]::Replace($Text, $artifact, 'releases/')
    foreach ($variant in @($ProjectRoot.Replace('\','\\'), $ProjectRoot.Replace('\','/'), $ProjectRoot) | Select-Object -Unique) {
        if ($variant) { $Text = $Text.Replace($variant, '<project-root>') }
    }
    foreach ($entry in @(@('Local','%LOCALAPPDATA%'), @('Roaming','%APPDATA%'))) {
        $Text = [regex]::Replace($Text, $userPrefix + $separator + 'AppData' + $separator + $entry[0], $entry[1])
    }
    $Text = [regex]::Replace($Text, $userPrefix, '%USERPROFILE%')
    $Text = [regex]::Replace($Text, '(?i)/(?:home|Users)/[^/\s"<>]+', '$HOME')
    return $Text
}

function Test-PrivatePath {
    param([AllowEmptyString()][string]$Text)
    $pattern = '(?i)(?<![a-z0-9])(?:[a-z]:[\\/]+Users[\\/]+(?!Public(?:[\\/]|$))[^\\/\s"<>]+|/(?:home|Users)/[^/\s"<>]+)'
    return [regex]::IsMatch($Text, $pattern)
}
