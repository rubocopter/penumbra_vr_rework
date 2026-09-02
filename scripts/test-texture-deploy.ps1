[CmdletBinding()]
param()
# Installer integration test against a disposable fixture, never the real game.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$fixture=Join-Path $repoRoot ('build\texture-deploy-test-'+[Guid]::NewGuid().ToString('N'))
$package=Join-Path $fixture 'package'
$game=Join-Path $fixture 'game'
$redist=Join-Path $game 'redist'
foreach($path in @($package,$redist,(Join-Path $redist 'config'),(Join-Path $package 'docs'),(Join-Path $package 'vr'))) {
    New-Item -ItemType Directory -Path $path -Force | Out-Null
}
# Installer validates file existence/hashes, not executable/image contents.
$original=Join-Path $repoRoot 'readme.md'
$originalHash=(Get-FileHash -LiteralPath $original).Hash
Copy-Item -LiteralPath $original -Destination (Join-Path $redist 'Penumbra.exe')
Copy-Item -LiteralPath $original -Destination (Join-Path $package 'Penumbra_vr.exe')
Copy-Item -LiteralPath $original -Destination (Join-Path $package 'openvr_api.dll')
Copy-Item -LiteralPath (Join-Path $repoRoot 'data\config\English.lang') -Destination (Join-Path $redist 'config\English.lang')
Copy-Item -LiteralPath (Join-Path $repoRoot 'data\vr\actions.json') -Destination (Join-Path $package 'vr\actions.json')
Copy-Item -LiteralPath (Join-Path $PSScriptRoot 'deploy.ps1') -Destination $fixture
$manifestPath=Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json'
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $package 'docs\TEXTURE_SELECTION.json')
$selection=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$policy=Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_POLICY.json') | ConvertFrom-Json
$legacyFiles=@($selection.Files | Where-Object { $_.Path -in $policy.RetainSourcePaths })
foreach($file in $selection.Files) {
    foreach($root in @($package,$redist)) { New-Item -ItemType Directory -Path (Split-Path -Parent (Join-Path $root $file.Path)) -Force | Out-Null }
    Copy-Item -LiteralPath $original -Destination (Join-Path $redist $file.Path)
    if ($file.Path -in $policy.RetainSourcePaths) {
        Copy-Item -LiteralPath (Join-Path $repoRoot ('data/'+$file.Path)) -Destination (Join-Path $package $file.Path)
    }
}
$fixtureInstaller=Join-Path $fixture 'deploy.ps1'
$argsForDeploy=@{PackageRoot=$package;InstallRoot=$game;SteamLauncher=$false}
# Start with the old nine-image manifest, then upgrade in-place. Original
# backups must survive the version change and the new models/ paths.
@{Version=1;Files=$legacyFiles} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $package 'docs\TEXTURE_SELECTION.json') -Encoding UTF8
& $fixtureInstaller @argsForDeploy 6>$null | Out-Null
foreach($file in $legacyFiles) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $file.SHA256) { throw 'Legacy install fixture failed.' }
}
& $fixtureInstaller @argsForDeploy -SkipTexturePack 6>$null | Out-Null
foreach($file in $legacyFiles) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $originalHash) { throw 'Legacy selective restore failed.' }
}
& $fixtureInstaller @argsForDeploy 6>$null | Out-Null
Copy-Item -LiteralPath $manifestPath -Destination (Join-Path $package 'docs\TEXTURE_SELECTION.json')
foreach($file in $selection.Files) {
    Copy-Item -LiteralPath (Join-Path $repoRoot ('data/'+$file.Path)) -Destination (Join-Path $package $file.Path)
}
& $fixtureInstaller @argsForDeploy 6>$null | Out-Null
foreach($file in $selection.Files) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $file.SHA256) { throw 'Texture installation fixture failed.' }
}
& $fixtureInstaller @argsForDeploy -SkipTexturePack 6>$null | Out-Null
foreach($file in $selection.Files) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $originalHash) { throw 'Texture-only restore fixture failed.' }
}
if (-not (Test-Path -LiteralPath (Join-Path $redist 'Penumbra_vr.exe'))) { throw 'Texture-only restore removed the mod.' }
& $fixtureInstaller @argsForDeploy 6>$null | Out-Null
foreach($file in $selection.Files) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $file.SHA256) { throw 'Texture re-enable fixture failed.' }
}
& $fixtureInstaller @argsForDeploy -Restore 6>$null | Out-Null
foreach($file in $selection.Files) {
    if ((Get-FileHash -LiteralPath (Join-Path $redist $file.Path)).Hash -ne $originalHash) { throw 'Full restore fixture failed.' }
}
if (Test-Path -LiteralPath (Join-Path $redist 'Penumbra_vr.exe')) { throw 'Full restore left the fixture mod executable.' }
Write-Host "Texture fixture passed: legacy install/skip, upgrade, texture-only restore, re-enable and full restore ($($selection.Files.Count*4+2+$legacyFiles.Count*2) assertions)."
Write-Host "Fixture retained for inspection: $fixture"
