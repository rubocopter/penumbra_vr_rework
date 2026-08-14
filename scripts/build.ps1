[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Package,

    [switch]$Deploy,

    [string]$InstallRoot,

    [switch]$NoSteamLauncher
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'check-project.ps1')

$vswherePath = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
if (-not (Test-Path -LiteralPath $vswherePath)) {
    throw 'Visual Studio Installer was not found. Install Visual Studio 2022 Build Tools with the Desktop development with C++ workload.'
}

$msbuildPath = & $vswherePath `
    -latest `
    -products '*' `
    -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
    -find 'MSBuild\**\Bin\MSBuild.exe' |
    Select-Object -First 1

if (-not $msbuildPath) {
    throw 'MSBuild with the Visual C++ x86/x64 tools was not found. Install the Desktop development with C++ workload.'
}

$solutionPath = Join-Path $repositoryRoot 'PenumbraVR.sln'
$msbuildArguments = @(
    $solutionPath,
    '/m',
    '/nr:false',
    # Several game objects are allocated from translation units that only see
    # their class size through headers. The legacy project does not reliably
    # invalidate every dependent .obj after those layouts change, which can
    # mix an old allocation size with a new constructor and corrupt the heap.
    '/t:Rebuild',
    "/p:Configuration=$Configuration",
    '/p:Platform=Win32',
    '/p:PreferredToolArchitecture=x64',
    '/nologo',
    '/verbosity:minimal'
)

Write-Host "Rebuilding Penumbra VR ($Configuration|Win32) with $msbuildPath"
& $msbuildPath $msbuildArguments
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

$executablePath = Join-Path $repositoryRoot "build\bin\$Configuration\Penumbra_vr.exe"
$peBytes = [System.IO.File]::ReadAllBytes($executablePath)
$peHeaderOffset = [System.BitConverter]::ToInt32($peBytes, 0x3c)
$characteristics = [System.BitConverter]::ToUInt16($peBytes, $peHeaderOffset + 22)
if (($characteristics -band 0x20) -eq 0) {
    throw "Built executable is missing IMAGE_FILE_LARGE_ADDRESS_AWARE: $executablePath"
}
Write-Host 'Verified Large Address Aware in the executable PE header.' -ForegroundColor Green

if ($Package -or $Deploy) {
    & (Join-Path $PSScriptRoot 'package.ps1') -Configuration $Configuration
}

if ($Deploy) {
    $deployArguments = @{
        Configuration = $Configuration
        SteamLauncher = -not $NoSteamLauncher
    }
    if ($InstallRoot) {
        $deployArguments.InstallRoot = $InstallRoot
    }
    & (Join-Path $PSScriptRoot 'deploy.ps1') @deployArguments
}
