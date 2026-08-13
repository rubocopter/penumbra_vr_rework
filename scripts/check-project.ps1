[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$requiredPaths = @(
    'PenumbraVR.sln',
    'OALWrapper\OALWrapper.vcxproj',
    'HPL1Engine\HPL.vcxproj',
    'PenumbraOverture\Penumbra.vcxproj',
    'dependencies\openvr-1.0.2\headers\openvr.h',
    'dependencies\openvr-1.0.2\lib\win32\openvr_api.lib',
    'dependencies\openvr-1.0.2\bin\win32\openvr_api.dll',
    'dependencies\lib\win32\Newton.dll',
    'dependencies\lib\win32\SDL.dll',
    'data\config\English.lang',
    'data\config\Espanol.lang'
)

$missingPaths = foreach ($relativePath in $requiredPaths) {
    $candidatePath = Join-Path $repositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidatePath)) {
        $relativePath
    }
}

if ($missingPaths) {
    throw "Missing required repository files:`n - $($missingPaths -join "`n - ")"
}

$projectPaths = @(
    'OALWrapper\OALWrapper.vcxproj',
    'HPL1Engine\HPL.vcxproj',
    'PenumbraOverture\Penumbra.vcxproj'
)

foreach ($relativeProjectPath in $projectPaths) {
    $projectPath = Join-Path $repositoryRoot $relativeProjectPath
    [xml]$projectDocument = Get-Content -Raw -LiteralPath $projectPath
    $projectText = $projectDocument.OuterXml

    if ($projectText -match '(?i)[A-Z]:\\dev\\vrpenumbra') {
        throw "Author-specific absolute path remains in $relativeProjectPath"
    }
    if ($projectText -notmatch '<PlatformToolset>v143</PlatformToolset>') {
        throw "Visual Studio 2022 toolset is not configured in $relativeProjectPath"
    }
    if ($projectText -notmatch '<Platform>Win32</Platform>') {
        throw "The required Win32 target is missing from $relativeProjectPath"
    }
}

function Get-LanguageEntryMap([string]$relativePath) {
    $languagePath = Join-Path $repositoryRoot $relativePath
    $languageDocument = New-Object System.Xml.XmlDocument
    $languageDocument.Load($languagePath)
    $entries = @{}

    foreach ($category in $languageDocument.SelectNodes('/LANGUAGE/CATEGORY')) {
        foreach ($entry in $category.SelectNodes('Entry')) {
            $key = $category.GetAttribute('Name') + '/' + $entry.GetAttribute('Name')
            $entries[$key] = $entry.InnerText
        }
    }

    return $entries
}

$englishEntries = Get-LanguageEntryMap 'data\config\English.lang'
$spanishEntries = Get-LanguageEntryMap 'data\config\Espanol.lang'
$missingSpanishEntries = @($englishEntries.Keys | Where-Object { -not $spanishEntries.ContainsKey($_) } | Sort-Object)
$unexpectedSpanishEntries = @($spanishEntries.Keys | Where-Object { -not $englishEntries.ContainsKey($_) } | Sort-Object)

if ($missingSpanishEntries -or $unexpectedSpanishEntries) {
    $details = @()
    if ($missingSpanishEntries) {
        $details += "Missing Spanish entries:`n - $($missingSpanishEntries -join "`n - ")"
    }
    if ($unexpectedSpanishEntries) {
        $details += "Unexpected Spanish entries:`n - $($unexpectedSpanishEntries -join "`n - ")"
    }
    throw "English and Spanish language keys do not match.`n$($details -join "`n")"
}

Write-Host 'Project validation passed.' -ForegroundColor Green
