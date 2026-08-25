[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [switch]$Package,

    [switch]$Deploy,

    [string]$InstallRoot,

    [switch]$NoSteamLauncher,

    # Force the conservative full rebuild even when no header or project input
    # changed since the last successful build.
    [switch]$Full
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot 'check-project.ps1')

# Fingerprint of every input that can change class layouts or compile flags:
# headers across the tree plus the project files themselves. Source bodies
# (.cpp) are deliberately excluded because reusing their object files is safe
# as long as the headers they saw did not change.
function Get-BuildInputFingerprint([string]$RepositoryRoot) {
    $excludedRoots = @('.git', '.vs', 'build')
    $inputFiles = Get-ChildItem -LiteralPath $RepositoryRoot -Recurse -File -Include '*.h', '*.hpp', '*.inl', '*.ipp', '*.vcxproj' |
        Where-Object {
            $firstSegment = $_.FullName.Substring($RepositoryRoot.Length + 1).Split('\')[0]
            $firstSegment -notin $excludedRoots
        } |
        Sort-Object FullName

    $fingerprintInput = $inputFiles | ForEach-Object {
        '{0}|{1}' -f $_.FullName.Substring($RepositoryRoot.Length + 1), $_.LastWriteTimeUtc.Ticks
    }
    $joinedInput = $fingerprintInput -join "`n"
    $sha256 = [System.Security.Cryptography.SHA256]::Create()
    try {
        $hashBytes = $sha256.ComputeHash([System.Text.Encoding]::UTF8.GetBytes($joinedInput))
        return [System.BitConverter]::ToString($hashBytes).Replace('-', '').ToLowerInvariant()
    } finally {
        $sha256.Dispose()
    }
}

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
$executablePath = Join-Path $repositoryRoot "build\bin\$Configuration\Penumbra_vr.exe"
$stampPath = Join-Path $repositoryRoot "build\.rebuild-guard-$Configuration"

# The legacy project does not reliably invalidate every dependent .obj after
# class layouts change through headers, which can mix an old allocation size
# with a new constructor and corrupt the heap. A full rebuild is therefore
# mandatory whenever any header or project file changed since the last
# successful build; otherwise the existing objects already agree with the
# current headers and an incremental build is safe.
$fingerprint = Get-BuildInputFingerprint -RepositoryRoot $repositoryRoot
$storedFingerprint = $null
if (Test-Path -LiteralPath $stampPath) {
    $storedFingerprint = (Get-Content -Raw -LiteralPath $stampPath).Trim()
}
$canBuildIncrementally = -not $Full -and
    (Test-Path -LiteralPath $executablePath) -and
    ($storedFingerprint -eq $fingerprint)
$buildTarget = if ($canBuildIncrementally) { '/t:Build' } else { '/t:Rebuild' }

$msbuildArguments = @(
    $solutionPath,
    '/m',
    '/nr:false',
    $buildTarget,
    "/p:Configuration=$Configuration",
    '/p:Platform=Win32',
    '/p:PreferredToolArchitecture=x64',
    '/nologo',
    '/verbosity:minimal'
)

$modeLabel = if ($canBuildIncrementally) { 'incremental, header inputs unchanged' } else { 'full rebuild, header or project inputs changed' }
Write-Host "Building Penumbra VR ($Configuration|Win32): $modeLabel"
$buildStopwatch = [System.Diagnostics.Stopwatch]::StartNew()
& $msbuildPath $msbuildArguments
if ($LASTEXITCODE -ne 0) {
    throw "MSBuild failed with exit code $LASTEXITCODE"
}
$buildStopwatch.Stop()
Write-Host ('Build finished in {0:mm\:ss} ({1}).' -f $buildStopwatch.Elapsed, $buildTarget)

# Record the fingerprint only after a successful build so a failed or
# interrupted rebuild falls back to the conservative full path next time.
$stampDirectory = Split-Path -Parent $stampPath
if (-not (Test-Path -LiteralPath $stampDirectory)) {
    New-Item -ItemType Directory -Path $stampDirectory -Force | Out-Null
}
Set-Content -LiteralPath $stampPath -Value $fingerprint -Encoding ascii

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
