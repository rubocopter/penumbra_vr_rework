[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Package
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
    '/t:Build',
    "/p:Configuration=$Configuration",
    '/p:Platform=Win32',
    '/p:PreferredToolArchitecture=x64',
    '/nologo',
    '/verbosity:minimal'
)

Write-Host "Building Penumbra VR ($Configuration|Win32) with $msbuildPath"
& $msbuildPath $msbuildArguments
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}

if ($Package) {
    & (Join-Path $PSScriptRoot 'package.ps1') -Configuration $Configuration
}
