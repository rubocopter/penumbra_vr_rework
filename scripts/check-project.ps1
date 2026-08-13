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
    'dependencies\openvr-2.15.6\headers\openvr.h',
    'dependencies\openvr-2.15.6\lib\win32\openvr_api.lib',
    'dependencies\openvr-2.15.6\bin\win32\openvr_api.dll',
    'dependencies\lib\win32\Newton.dll',
    'dependencies\lib\win32\SDL.dll',
    'data\config\English.lang',
    'data\config\Espanol.lang',
    'data\vr\actions.json',
    'data\vr\bindings\psvr2_sense.json',
    'data\vr\bindings\vive_controller.json'
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

$literalTranslationKeys = @{}
$translationPattern = 'kTranslate\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)'
Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture') -Recurse -Include '*.cpp', '*.h' -File |
    ForEach-Object {
        $sourceText = Get-Content -Raw -LiteralPath $_.FullName
        foreach ($match in [regex]::Matches($sourceText, $translationPattern)) {
            $key = $match.Groups[1].Value + '/' + $match.Groups[2].Value
            $literalTranslationKeys[$key] = $true
        }
    }

$missingLiteralTranslations = @($literalTranslationKeys.Keys | Where-Object {
    -not $englishEntries.ContainsKey($_) -or -not $spanishEntries.ContainsKey($_)
} | Sort-Object)
if ($missingLiteralTranslations) {
    throw "Literal translations used by the game are missing:`n - $($missingLiteralTranslations -join "`n - ")"
}

$vrDataRoot = Join-Path $repositoryRoot 'data\vr'
$actionManifestPath = Join-Path $vrDataRoot 'actions.json'
$actionManifest = Get-Content -Raw -LiteralPath $actionManifestPath | ConvertFrom-Json
$actionNames = @($actionManifest.actions | ForEach-Object { $_.name })
$actionSetNames = @($actionManifest.action_sets | ForEach-Object { $_.name })

if (($actionNames | Sort-Object -Unique).Count -ne $actionNames.Count) {
    throw 'Duplicate SteamVR action names were found in data\vr\actions.json.'
}
if (($actionSetNames | Sort-Object -Unique).Count -ne $actionSetNames.Count) {
    throw 'Duplicate SteamVR action set names were found in data\vr\actions.json.'
}

foreach ($localization in $actionManifest.localization) {
    $localizedNames = @($localization.PSObject.Properties.Name)
    foreach ($requiredName in @($actionSetNames + $actionNames)) {
        if ($localizedNames -notcontains $requiredName) {
            throw "SteamVR localization '$($localization.language_tag)' is missing '$requiredName'."
        }
    }
}

foreach ($defaultBinding in $actionManifest.default_bindings) {
    $bindingRelativePath = $defaultBinding.binding_url.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $bindingPath = Join-Path $vrDataRoot $bindingRelativePath
    if (-not (Test-Path -LiteralPath $bindingPath)) {
        throw "SteamVR default binding does not exist: $bindingRelativePath"
    }

    $binding = Get-Content -Raw -LiteralPath $bindingPath | ConvertFrom-Json
    if ($binding.controller_type -ne $defaultBinding.controller_type) {
        throw "SteamVR binding controller type mismatch in $bindingRelativePath."
    }

    foreach ($bindingSetProperty in $binding.bindings.PSObject.Properties) {
        if ($actionSetNames -notcontains $bindingSetProperty.Name) {
            throw "Unknown SteamVR action set '$($bindingSetProperty.Name)' in $bindingRelativePath."
        }

        $bindingSet = $bindingSetProperty.Value
        foreach ($directBindingCollection in @('haptics', 'poses', 'skeleton')) {
            foreach ($directBinding in @($bindingSet.$directBindingCollection)) {
                if ($null -ne $directBinding -and $actionNames -notcontains $directBinding.output) {
                    throw "Unknown SteamVR action '$($directBinding.output)' in $bindingRelativePath."
                }
            }
        }

        foreach ($source in @($bindingSet.sources)) {
            if ($null -eq $source) {
                continue
            }
            foreach ($inputProperty in $source.inputs.PSObject.Properties) {
                $output = $inputProperty.Value.output
                if ($actionNames -notcontains $output) {
                    throw "Unknown SteamVR action '$output' in $bindingRelativePath."
                }
            }
        }
    }
}

Write-Host 'Project validation passed.' -ForegroundColor Green
