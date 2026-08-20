[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$buildRoot = [System.IO.Path]::GetFullPath((Join-Path $repositoryRoot 'build'))
$packageRoot = [System.IO.Path]::GetFullPath((Join-Path $buildRoot "package\$Configuration\PenumbraVR"))
$expectedPrefix = $buildRoot.TrimEnd([System.IO.Path]::DirectorySeparatorChar) + [System.IO.Path]::DirectorySeparatorChar

if (-not $packageRoot.StartsWith($expectedPrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
    throw "Refusing to package outside the build directory: $packageRoot"
}

$executablePath = Join-Path $buildRoot "bin\$Configuration\Penumbra_vr.exe"
if (-not (Test-Path -LiteralPath $executablePath)) {
    throw "Build output not found: $executablePath"
}

if (Test-Path -LiteralPath $packageRoot) {
    Remove-Item -LiteralPath $packageRoot -Recurse -Force
}
New-Item -ItemType Directory -Path $packageRoot -Force | Out-Null

Copy-Item -LiteralPath $executablePath -Destination $packageRoot
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\openvr-2.15.6\bin\win32\openvr_api.dll') -Destination $packageRoot
$dataRoot = Join-Path $repositoryRoot 'data'
Get-ChildItem -LiteralPath $dataRoot -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Recurse
}
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'readme.md') -Destination (Join-Path $packageRoot 'README.md')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\COPYING') -Destination (Join-Path $packageRoot 'COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\deploy.ps1') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.ps1')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\Install-PenumbraVR.bat') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.bat')

foreach ($requiredPath in @('Penumbra_vr.exe', 'openvr_api.dll', 'Install-PenumbraVR.ps1', 'Install-PenumbraVR.bat', 'config\English.lang', 'config\Espanol.lang', 'docs\INPUT.md', 'docs\ROADMAP.md', 'maps', 'models', 'vr\actions.json', 'vr\bindings\psvr2_sense.json', 'vr\bindings\vive_controller.json', 'vr\bindings\knuckles.json', 'vr\bindings\oculus_touch.json', 'vr\bindings\microsoft_motion_controller.json')) {
    $packagedPath = Join-Path $packageRoot $requiredPath
    if (-not (Test-Path -LiteralPath $packagedPath)) {
        throw "Required package entry was not created: $packagedPath"
    }
}

if (Test-Path -LiteralPath (Join-Path $packageRoot 'data')) {
    throw "Invalid package layout: data must be merged into the game redist root."
}

$manifestPath = Join-Path $packageRoot 'SHA256SUMS.txt'
function Get-RelativePath([string]$basePath, [string]$fullPath) {
    # Path.GetRelativePath only exists on .NET Framework 4.7.1+; Uri works
    # since .NET Framework 4.0 and keeps package.ps1 usable on any runtime.
    $base = [System.IO.Path]::GetFullPath($basePath).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $baseUri = [System.Uri]($base + [System.IO.Path]::DirectorySeparatorChar)
    $fullUri = [System.Uri]([System.IO.Path]::GetFullPath($fullPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fullUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}
$manifestLines = Get-ChildItem -LiteralPath $packageRoot -File -Recurse |
    Where-Object { $_.FullName -ne $manifestPath } |
    Sort-Object FullName |
    ForEach-Object {
        $relativePath = (Get-RelativePath $packageRoot $_.FullName).Replace('\', '/')
        $hash = (Get-FileHash -LiteralPath $_.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
        "$hash  $relativePath"
    }
$manifestLines | Set-Content -LiteralPath $manifestPath -Encoding utf8

Write-Host "Package created at $packageRoot" -ForegroundColor Green
