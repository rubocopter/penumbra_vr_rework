[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot
)
# Import only the individually reviewed world diffuse images. Never overlay
# the whole pack: its HUD/material/normal-map assets are not VR compatible.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
$source=(Resolve-Path -LiteralPath $SourceRoot).Path
$game=(Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$selected=@(
    'textures/iron_mine/iron_mine_briks.jpg',
    'textures/iron_mine/iron_mine_pillar.jpg',
    'textures/iron_mine/iron_mine_pipe_end.jpg',
    'textures/locker_room/locker_room_walltiles.jpg',
    'textures/locker_room/locker_room_walltiles_dirty.jpg',
    'textures/modern_mine/modern_mine_bluemetal.jpg',
    'textures/modern_mine/modern_mine_green_metal.jpg',
    'textures/modern_mine/modern_mine_pipe.jpg',
    'textures/new_storage_room/new_storage_room_rust.jpg'
)
& (Join-Path $PSScriptRoot 'audit-texture-pack.ps1') -SourceRoot $source -GameRoot $game | Out-Null
$inventory=Get-Content -Raw (Join-Path $repoRoot 'build\texture-audit\inventory.json') | ConvertFrom-Json
$materials=@(Get-ChildItem -LiteralPath $game -Recurse -File -Filter '*.mat')
$records=@(foreach ($path in $selected) {
    $row=@($inventory | Where-Object Path -eq $path)
    if ($row.Count -ne 1 -or $row[0].Status -ne 'candidate') { throw "Texture no longer eligible: $path" }
    $row=$row[0]
    $name=[IO.Path]::GetFileName($path)
    $reviewed=@(foreach ($material in $materials) {
        $raw=Get-Content -Raw -LiteralPath $material.FullName
        if ($raw -notmatch [regex]::Escape($name)) { continue }
        $xml=[xml]$raw
        foreach ($unit in $xml.SelectNodes('/Material/TextureUnits/*')) {
            if ([IO.Path]::GetFileName($unit.GetAttribute('File')) -ine $name) { continue }
            $main=$xml.SelectSingleNode('/Material/Main')
            if ($unit.LocalName -ne 'Diffuse' -or $unit.GetAttribute('Type') -ne '2D' -or
                $unit.GetAttribute('AnimMode') -ne 'None' -or $unit.GetAttribute('Mipmaps') -ne 'true' -or
                $main.GetAttribute('Type') -notin @('Diffuse','Bump','BumpSpecular','BumpColorSpecular','DiffuseSpecular') -or
                $main.GetAttribute('UseAlpha') -ne 'False' -or $main.GetAttribute('DepthTest') -ne 'True') {
                throw "Non-opaque/static diffuse use found: $path in $($material.FullName)"
            }
            $material.FullName.Substring($game.Length+1).Replace('\','/')
        }
    })
    if ($reviewed.Count -eq 0) { throw "No reviewed material: $path" }
    [ordered]@{
        Path=$path; Width=$row.Width; Height=$row.Height
        StockWidth=$row.OriginalWidth; StockHeight=$row.OriginalHeight
        EstimatedExtraRGBABytes=$row.EstimatedExtraRGBABytes
        Bytes=(Get-Item -LiteralPath (Join-Path $source $path)).Length
        SHA256=(Get-FileHash -LiteralPath (Join-Path $source $path)).Hash
        StockSHA256=(Get-FileHash -LiteralPath (Join-Path $game $path)).Hash
        ReviewedMaterials=$reviewed
    }
})
$total=($records | ForEach-Object { $_.EstimatedExtraRGBABytes } | Measure-Object -Sum).Sum
if ($total -gt 40MB) { throw 'Selection exceeds the 40 MiB incremental RGBA+mips budget.' }
# Validate every destination before any copy, preserving pre-existing edits.
foreach ($record in $records) {
    $dest=Join-Path $repoRoot ('data/'+$record.Path)
    if ((Test-Path -LiteralPath $dest) -and (Get-FileHash -LiteralPath $dest).Hash -ne $record.SHA256) {
        throw "Refusing to overwrite a different project texture: $dest"
    }
}
foreach ($record in $records) {
    $dest=Join-Path $repoRoot ('data/'+$record.Path)
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $source $record.Path) -Destination $dest
}
[ordered]@{
    Version=1; SourceName='Penumbra Collection 4x AI upscale'
    SourceURL='https://www.nexusmods.com/penumbraoverture/mods/5?tab=description'
    Creator='Bret2011'; Uploader='c3pohumancyborgrelations'; OriginalAssetCredit='Frictional Games'
    PermissionsChecked='2026-09-01'
    Distribution='Pack permits redistribution with creator credit; see TEXTURE_CREDITS.md. Not relicensed as GPL.'
    MaxDimension=1024; ExtraRGBABytesBudget=40MB; EstimatedExtraRGBABytes=$total
    Files=$records
} | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') -Encoding UTF8
Write-Host "Imported $($records.Count) reviewed diffuse textures; estimated incremental RGBA+mips: $($total/1MB) MiB."
