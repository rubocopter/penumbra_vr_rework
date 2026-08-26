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
# App-local OpenAL Soft: guarantees EFX (filters/reverb) and HRTF support on
# every machine regardless of which legacy OpenAL runtime is installed. The
# DLL search order picks this copy up before any system-wide router.
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenAL32.dll') -Destination $packageRoot
# App-local Visual C++ runtime: the executable links the DLL CRT, so ship the
# redistributable DLLs next to it instead of asking players to install the
# VC++ redist (officially supported app-local deployment). SteamVR already
# requires Windows 10/11, where only these two files are ever missing.
$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$crtDlls = @()
if (Test-Path -LiteralPath $vswherePath) {
    $crtDlls = & $vswherePath -latest -products * -find 'VC\Redist\MSVC\*\x86\Microsoft.VC143.CRT\msvcp140.dll' | Select-Object -First 1
}
if (-not ($crtDlls -and (Test-Path -LiteralPath $crtDlls))) {
    throw 'Visual C++ 2022 redist x86 DLLs were not found; install the VC++ tools workload that provides VC\Redist.'
}
$crtDirectory = Split-Path -Parent $crtDlls
foreach ($crtDll in @('msvcp140.dll', 'vcruntime140.dll')) {
    Copy-Item -LiteralPath (Join-Path $crtDirectory $crtDll) -Destination $packageRoot
}
New-Item -ItemType Directory -Path (Join-Path $packageRoot 'licenses') -Force | Out-Null
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenALSoft-COPYING') -Destination (Join-Path $packageRoot 'licenses\OpenALSoft-COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'dependencies\bin\win32\OpenALSoft-readme.txt') -Destination (Join-Path $packageRoot 'licenses\OpenALSoft-readme.txt')
$dataRoot = Join-Path $repositoryRoot 'data'
Get-ChildItem -LiteralPath $dataRoot -Force | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $packageRoot -Recurse
}
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'readme.md') -Destination (Join-Path $packageRoot 'README.md')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\COPYING') -Destination (Join-Path $packageRoot 'COPYING.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'docs') -Destination $packageRoot -Recurse
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\deploy.ps1') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.ps1')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'scripts\Install-PenumbraVR.bat') -Destination (Join-Path $packageRoot 'Install-PenumbraVR.bat')

foreach ($requiredPath in @('Penumbra_vr.exe', 'openvr_api.dll', 'OpenAL32.dll', 'msvcp140.dll', 'vcruntime140.dll', 'licenses\OpenALSoft-COPYING.txt', 'Install-PenumbraVR.ps1', 'Install-PenumbraVR.bat', 'config\English.lang', 'config\Espanol.lang', 'docs\INPUT.md', 'docs\ROADMAP.md', 'maps', 'models', 'vr\actions.json', 'vr\bindings\psvr2_sense.json', 'vr\bindings\vive_controller.json', 'vr\bindings\knuckles.json', 'vr\bindings\oculus_touch.json', 'vr\bindings\microsoft_motion_controller.json', 'vr\bindings\pico4_controller.json', 'vr\bindings\pico_neo3_controller.json', 'vr\bindings\holographic_controller.json')) {
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
