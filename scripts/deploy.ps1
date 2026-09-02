[CmdletBinding()]
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Release',

    [string]$PackageRoot,

    [string]$InstallRoot,

    [bool]$SteamLauncher = $true,

    [switch]$Restore,

    # Also restores these textures from deployment backups when already installed.
    [switch]$SkipTexturePack
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-NormalizedRoot([string]$path) {
    return [System.IO.Path]::GetFullPath($path).TrimEnd(
        [System.IO.Path]::DirectorySeparatorChar,
        [System.IO.Path]::AltDirectorySeparatorChar)
}

function Get-PathUnderRoot([string]$root, [string]$relativePath) {
    if ([System.IO.Path]::IsPathRooted($relativePath)) {
        throw "Managed path must be relative: $relativePath"
    }

    $normalizedRoot = Get-NormalizedRoot $root
    $candidate = [System.IO.Path]::GetFullPath((Join-Path $normalizedRoot $relativePath))
    $prefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $candidate.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Managed path escapes the target root: $relativePath"
    }
    return $candidate
}

function Get-FileHashValue([string]$path) {
    if (-not (Test-Path -LiteralPath $path -PathType Leaf)) {
        return $null
    }
    return (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant()
}

function Get-RelativePath([string]$basePath, [string]$fullPath) {
    # Path.GetRelativePath only exists on .NET Framework 4.7.1+; Windows
    # PowerShell 5.1 on a legacy .NET Framework does not provide it. Uri is
    # available since .NET Framework 4.0, so this works everywhere.
    $base = [System.IO.Path]::GetFullPath($basePath).TrimEnd([System.IO.Path]::DirectorySeparatorChar)
    $baseUri = [System.Uri]($base + [System.IO.Path]::DirectorySeparatorChar)
    $fullUri = [System.Uri]([System.IO.Path]::GetFullPath($fullPath))
    return [System.Uri]::UnescapeDataString($baseUri.MakeRelativeUri($fullUri).ToString()).Replace('/', [System.IO.Path]::DirectorySeparatorChar)
}

function Get-SteamRoots {
    $roots = New-Object System.Collections.Generic.List[string]

    $registryPaths = @(
        'HKCU:\Software\Valve\Steam',
        'HKLM:\Software\WOW6432Node\Valve\Steam'
    )
    foreach ($registryPath in $registryPaths) {
        $steamProperties = Get-ItemProperty -LiteralPath $registryPath -ErrorAction SilentlyContinue
        if ($null -ne $steamProperties) {
            foreach ($propertyName in @('SteamPath', 'InstallPath')) {
                $property = $steamProperties.PSObject.Properties[$propertyName]
                if ($null -ne $property -and $property.Value) {
                    $roots.Add([string]$property.Value)
                }
            }
        }
    }

    if (${env:ProgramFiles(x86)}) {
        $roots.Add((Join-Path ${env:ProgramFiles(x86)} 'Steam'))
    }

    $expandedRoots = New-Object System.Collections.Generic.List[string]
    foreach ($root in @($roots | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $root -PathType Container)) {
            continue
        }
        $expandedRoots.Add($root)

        $libraryFile = Join-Path $root 'steamapps\libraryfolders.vdf'
        if (Test-Path -LiteralPath $libraryFile -PathType Leaf) {
            $libraryText = Get-Content -Raw -LiteralPath $libraryFile
            foreach ($match in [regex]::Matches($libraryText, '"path"\s+"([^"]+)"')) {
                $libraryRoot = $match.Groups[1].Value.Replace('\\', '\')
                if (Test-Path -LiteralPath $libraryRoot -PathType Container) {
                    $expandedRoots.Add($libraryRoot)
                }
            }
        }
    }

    return @($expandedRoots | Select-Object -Unique)
}

function Resolve-GamePaths([string]$requestedRoot) {
    $candidates = New-Object System.Collections.Generic.List[string]
    if ($requestedRoot) {
        $candidates.Add($requestedRoot)
    }
    else {
        foreach ($steamRoot in Get-SteamRoots) {
            $candidates.Add((Join-Path $steamRoot 'steamapps\common\Penumbra Overture'))
        }
    }

    foreach ($candidate in @($candidates | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $candidate -PathType Container)) {
            continue
        }

        $normalized = Get-NormalizedRoot $candidate
        if ((Split-Path -Leaf $normalized) -ieq 'redist') {
            $gameRoot = Split-Path -Parent $normalized
            $redistRoot = $normalized
        }
        else {
            $gameRoot = $normalized
            $redistRoot = Join-Path $gameRoot 'redist'
        }

        if ((Test-Path -LiteralPath (Join-Path $redistRoot 'Penumbra.exe') -PathType Leaf) -and
            (Test-Path -LiteralPath (Join-Path $redistRoot 'config\English.lang') -PathType Leaf)) {
            return [pscustomobject]@{
                GameRoot = Get-NormalizedRoot $gameRoot
                RedistRoot = Get-NormalizedRoot $redistRoot
            }
        }
    }

    if ($requestedRoot) {
        throw "The requested path is not a Penumbra: Overture installation: $requestedRoot"
    }
    throw 'Could not locate the Steam installation of Penumbra: Overture. Pass -InstallRoot explicitly.'
}

function Resolve-PackageRoot([string]$requestedRoot, [string]$configuration) {
    if ($requestedRoot) {
        $candidate = Get-NormalizedRoot $requestedRoot
    }
    elseif (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'Penumbra_vr.exe') -PathType Leaf) {
        # The installer is running from a packaged release.
        $candidate = Get-NormalizedRoot $PSScriptRoot
    }
    else {
        $repositoryRoot = Split-Path -Parent $PSScriptRoot
        $candidate = Get-NormalizedRoot (Join-Path $repositoryRoot "build\package\$configuration\PenumbraVR")
    }

    foreach ($requiredFile in @('Penumbra_vr.exe', 'openvr_api.dll', 'vr\actions.json')) {
        if (-not (Test-Path -LiteralPath (Join-Path $candidate $requiredFile) -PathType Leaf)) {
            throw "Package is incomplete; missing '$requiredFile' under $candidate"
        }
    }

    # Guard against deploying a stale package: if any engine or game source
    # file is newer than the packaged executable, packaging was skipped after
    # the last build and this deploy would silently ship old code.
    if ($candidate -like (Join-Path (Split-Path -Parent $PSScriptRoot) 'build*')) {
        $packagedExe = Get-Item -LiteralPath (Join-Path $candidate 'Penumbra_vr.exe')
        $sourceRoots = @('PenumbraOverture', 'HPL1Engine') | ForEach-Object {
            Join-Path (Split-Path -Parent $PSScriptRoot) $_
        }
        $newestSource = Get-ChildItem $sourceRoots -Recurse -Include *.cpp, *.hpp, *.h -ErrorAction SilentlyContinue |
            Sort-Object LastWriteTime -Descending |
            Select-Object -First 1
        if ($newestSource -and $newestSource.LastWriteTime -gt $packagedExe.LastWriteTime) {
            throw ("Package is stale: '{0}' ({1}) is newer than the packaged exe ({2}). " +
                   "Run scripts/build.ps1 -Deploy instead of deploy.ps1 alone.") -
                  $newestSource.Name, $newestSource.LastWriteTime, $packagedExe.LastWriteTime
        }
    }
    return $candidate
}

function Remove-EmptyManagedParents([string]$filePath, [string]$managedRoot) {
    $normalizedRoot = Get-NormalizedRoot $managedRoot
    $directory = Split-Path -Parent $filePath
    while ($directory -and $directory -ne $normalizedRoot) {
        $prefix = $normalizedRoot + [System.IO.Path]::DirectorySeparatorChar
        if (-not $directory.StartsWith($prefix, [System.StringComparison]::OrdinalIgnoreCase)) {
            throw "Refusing to remove a directory outside the managed root: $directory"
        }
        if (@(Get-ChildItem -LiteralPath $directory -Force).Count -ne 0) {
            break
        }
        Remove-Item -LiteralPath $directory -Force
        $directory = Split-Path -Parent $directory
    }
}

$paths = Resolve-GamePaths $InstallRoot
$gameRoot = $paths.GameRoot
$redistRoot = $paths.RedistRoot
$stateRoot = Join-Path $gameRoot '.penumbravr'
$statePath = Join-Path $stateRoot 'deploy-state.json'
$backupRoot = Join-Path $stateRoot 'backup'

$previousState = $null
if (Test-Path -LiteralPath $statePath -PathType Leaf) {
    $previousState = Get-Content -Raw -LiteralPath $statePath | ConvertFrom-Json
    if ([int]$previousState.Version -ne 1) {
        throw "Unsupported deployment state version in $statePath"
    }
    if ((Get-NormalizedRoot ([string]$previousState.RedistRoot)) -ne $redistRoot) {
        throw 'Deployment state belongs to a different Penumbra installation.'
    }
}

if ($Restore) {
    if ($null -eq $previousState) {
        throw "No Penumbra VR deployment state was found under $stateRoot"
    }

    foreach ($entry in $previousState.Files) {
        $relativePath = [string]$entry.Path
        $targetPath = Get-PathUnderRoot $redistRoot $relativePath
        $targetHash = Get-FileHashValue $targetPath
        $deployedHash = [string]$entry.DeployedHash

        if ([bool]$entry.HadOriginal) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
                throw "Cannot restore '$relativePath'; its original backup is missing."
            }
            $originalHash = [string]$entry.OriginalHash
            if ((Get-FileHashValue $backupPath) -ne $originalHash) {
                throw "Cannot restore '$relativePath'; its original backup failed hash validation."
            }
            if ($targetHash -and $targetHash -ne $deployedHash -and $targetHash -ne $originalHash) {
                throw "Refusing to overwrite externally modified managed file '$targetPath'."
            }
            $targetDirectory = Split-Path -Parent $targetPath
            New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
            Copy-Item -LiteralPath $backupPath -Destination $targetPath -Force
            Write-Host "Restored original: $relativePath"
        }
        elseif ($targetHash) {
            if ($targetHash -ne $deployedHash) {
                throw "Refusing to delete externally modified managed file '$targetPath'."
            }
            Remove-Item -LiteralPath $targetPath -Force
            Remove-EmptyManagedParents $targetPath $redistRoot
            Write-Host "Removed mod file: $relativePath"
        }
    }

    $resolvedStateRoot = Get-NormalizedRoot $stateRoot
    $expectedStatePrefix = $gameRoot + [System.IO.Path]::DirectorySeparatorChar
    if (-not $resolvedStateRoot.StartsWith($expectedStatePrefix, [System.StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to remove deployment state outside the game root: $resolvedStateRoot"
    }
    Remove-Item -LiteralPath $resolvedStateRoot -Recurse -Force
    Write-Host "Penumbra VR restored to its pre-deployment state at $redistRoot" -ForegroundColor Green
    return
}

$resolvedPackageRoot = Resolve-PackageRoot $PackageRoot $Configuration
$mappings = @{}
Get-ChildItem -LiteralPath $resolvedPackageRoot -File -Recurse | ForEach-Object {
    $relativePath = (Get-RelativePath $resolvedPackageRoot $_.FullName).Replace('\', '/')
    $mappings[$relativePath] = [pscustomobject]@{
        Path = $relativePath
        SourcePath = $_.FullName
    }
}

if ($SteamLauncher) {
    $mappings['Penumbra.exe'] = [pscustomobject]@{
        Path = 'Penumbra.exe'
        SourcePath = Join-Path $resolvedPackageRoot 'Penumbra_vr.exe'
    }
}

if ($SkipTexturePack) {
    $textureManifest = Join-Path $resolvedPackageRoot 'docs\TEXTURE_SELECTION.json'
    if (-not (Test-Path -LiteralPath $textureManifest)) { throw 'Texture selection manifest is missing.' }
    $selection = Get-Content -Raw -LiteralPath $textureManifest | ConvertFrom-Json
    if ($selection.Version -notin @(1,2)) { throw 'Unsupported texture selection manifest.' }
    foreach ($texture in $selection.Files) {
        $texturePath = [string]$texture.Path
        if ($texturePath -notmatch '^(textures|models)/([a-z0-9_]+/)+[a-z0-9_]+\.jpg$' -or
            $texturePath -match '^models/(hud_objects|items|player)/') { throw 'Invalid or protected selected texture path.' }
        $mappings.Remove($texturePath)
    }
    Write-Host 'Selected texture pack skipped; previous managed copies will be restored from backup.'
}

$previousEntries = @{}
if ($null -ne $previousState) {
    foreach ($entry in $previousState.Files) {
        $previousEntries[[string]$entry.Path] = $entry
    }
}

# Restore or remove only files recorded by the previous deployment. Never mirror
# the whole redist directory: nearly all files there belong to the commercial game.
foreach ($previousPath in @($previousEntries.Keys)) {
    if ($mappings.ContainsKey($previousPath)) {
        continue
    }

    $entry = $previousEntries[$previousPath]
    $targetPath = Get-PathUnderRoot $redistRoot $previousPath
    $targetHash = Get-FileHashValue $targetPath
    $deployedHash = [string]$entry.DeployedHash

    if ([bool]$entry.HadOriginal) {
        $backupPath = Get-PathUnderRoot $backupRoot $previousPath
        if (-not (Test-Path -LiteralPath $backupPath -PathType Leaf)) {
            throw "Cannot retire '$previousPath'; its original backup is missing."
        }
        $originalHash = [string]$entry.OriginalHash
        if ((Get-FileHashValue $backupPath) -ne $originalHash) {
            throw "Cannot retire '$previousPath'; its original backup failed hash validation."
        }
        if ($targetHash -and $targetHash -ne $deployedHash -and $targetHash -ne $originalHash) {
            throw "Refusing to overwrite externally modified stale file '$targetPath'."
        }
        $targetDirectory = Split-Path -Parent $targetPath
        New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
        Copy-Item -LiteralPath $backupPath -Destination $targetPath -Force
        Write-Host "Restored retired original: $previousPath"
    }
    elseif ($targetHash) {
        if ($targetHash -ne $deployedHash) {
            throw "Refusing to delete externally modified stale file '$targetPath'."
        }
        Remove-Item -LiteralPath $targetPath -Force
        Remove-EmptyManagedParents $targetPath $redistRoot
        Write-Host "Removed stale mod file: $previousPath"
    }
}

New-Item -ItemType Directory -Path $backupRoot -Force | Out-Null
$newEntries = New-Object System.Collections.Generic.List[object]
foreach ($mapping in @($mappings.Values | Sort-Object Path)) {
    $relativePath = [string]$mapping.Path
    $sourcePath = [string]$mapping.SourcePath
    $targetPath = Get-PathUnderRoot $redistRoot $relativePath
    $targetDirectory = Split-Path -Parent $targetPath
    $previousEntry = $previousEntries[$relativePath]

    if ($null -ne $previousEntry) {
        $hadOriginal = [bool]$previousEntry.HadOriginal
        $originalHash = if ($hadOriginal) { [string]$previousEntry.OriginalHash } else { $null }

        # Steam verification, a game update, or a deliberate local edit may have
        # replaced a managed target since the last deployment. Preserve that new
        # external version before synchronizing the current mod build over it.
        $currentTargetHash = Get-FileHashValue $targetPath
        $previousDeployedHash = [string]$previousEntry.DeployedHash
        if ($currentTargetHash -and $currentTargetHash -ne $previousDeployedHash) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            $backupDirectory = Split-Path -Parent $backupPath
            New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
            Copy-Item -LiteralPath $targetPath -Destination $backupPath -Force
            $hadOriginal = $true
            $originalHash = $currentTargetHash
            Write-Host "Refreshed external backup: $relativePath"
        }
    }
    else {
        $hadOriginal = Test-Path -LiteralPath $targetPath -PathType Leaf
        $originalHash = if ($hadOriginal) { Get-FileHashValue $targetPath } else { $null }
        if ($hadOriginal) {
            $backupPath = Get-PathUnderRoot $backupRoot $relativePath
            $backupDirectory = Split-Path -Parent $backupPath
            New-Item -ItemType Directory -Path $backupDirectory -Force | Out-Null
            Copy-Item -LiteralPath $targetPath -Destination $backupPath -Force
            Write-Host "Backed up original: $relativePath"
        }
    }

    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath -Force
    $deployedHash = Get-FileHashValue $targetPath
    $newEntries.Add([pscustomobject]@{
        Path = $relativePath
        HadOriginal = $hadOriginal
        OriginalHash = $originalHash
        DeployedHash = $deployedHash
    })
}

New-Item -ItemType Directory -Path $stateRoot -Force | Out-Null
$newState = [ordered]@{
    Version = 1
    RedistRoot = $redistRoot
    PackageRoot = $resolvedPackageRoot
    SteamLauncher = $SteamLauncher
    DeployedAt = (Get-Date).ToString('o')
    Files = $newEntries.ToArray()
}
$temporaryStatePath = Join-Path $stateRoot 'deploy-state.json.tmp'
$newState | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath $temporaryStatePath -Encoding utf8
Move-Item -LiteralPath $temporaryStatePath -Destination $statePath -Force

Write-Host "Penumbra VR synchronized to $redistRoot" -ForegroundColor Green
if ($SteamLauncher) {
    Write-Host 'Steam launcher integration is active: Steam will start the VR executable through Penumbra.exe.' -ForegroundColor Green
}
