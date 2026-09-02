[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'texture-pack-common.ps1')
$repoRoot = Split-Path -Parent $PSScriptRoot
$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$game = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$baseline = Get-TextureBaselineIndex $game
$gameFiles = @(Get-ChildItem -LiteralPath $game -Recurse -File)
$nameCounts = @{}
foreach ($entry in $gameFiles | Where-Object Extension -In '.jpg','.tga','.png','.dds','.bmp','.gif') {
    $key = $entry.BaseName.ToLowerInvariant()
    if (-not $nameCounts.ContainsKey($key)) { $nameCounts[$key] = 0 }
    $nameCounts[$key]++
}
# Every material use counts, including masks and special/transparent shaders.
$uses = @{}
foreach ($material in $gameFiles | Where-Object Extension -EQ '.mat') {
    $xml = [xml](Get-Content -Raw -LiteralPath $material.FullName)
    $main = $xml.SelectSingleNode('/Material/Main')
    foreach ($unit in $xml.SelectNodes('/Material/TextureUnits/*')) {
        $file = $unit.GetAttribute('File')
        if (-not $file) { continue }
        $key = [IO.Path]::GetFileNameWithoutExtension($file.Replace('\','/')).ToLowerInvariant()
        if (-not $uses.ContainsKey($key)) { $uses[$key] = @() }
        $safe = $null -ne $main -and $unit.LocalName -eq 'Diffuse' -and
            $unit.GetAttribute('Type') -eq '2D' -and $unit.GetAttribute('AnimMode') -in @('','None') -and
            $unit.GetAttribute('Mipmaps') -eq 'true' -and $main.GetAttribute('UseAlpha') -eq 'False' -and
            $main.GetAttribute('DepthTest') -eq 'True' -and
            $main.GetAttribute('Type') -in @('Diffuse','Bump','BumpSpecular','BumpColorSpecular','DiffuseSpecular')
        $uses[$key] += [pscustomobject]@{Material=$material.FullName.Substring($game.Length+1).Replace('\','/'); Unit=$unit.LocalName; Safe=$safe}
    }
}
# Allow the old pack selection; protect all other images owned by the VR mod.
$previous = Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') | ConvertFrom-Json
$previousPaths = @{}
foreach ($entry in $previous.Files) { $previousPaths[$entry.Path] = $true }
$protected = @{}
$dataRoot = Join-Path $repoRoot 'data'
foreach ($entry in Get-ChildItem -LiteralPath $dataRoot -Recurse -File) {
    $relative = $entry.FullName.Substring($dataRoot.Length+1).Replace('\','/')
    if ($entry.Extension -in '.jpg','.tga','.png','.dds','.bmp' -and -not $previousPaths.ContainsKey($relative)) { $protected[$entry.BaseName.ToLowerInvariant()] = $true }
}
# Held meshes can reference ordinary world images by basename. Inspect stock
# meshes too, because the mod still uses some of them through its .hud files.
$heldFiles = @($gameFiles | Where-Object { $_.FullName -match '\\models\\(hud_objects|player)\\' -and $_.Extension -in '.dae','.mat','.hud' })
$heldFiles += @(Get-ChildItem -LiteralPath (Join-Path $dataRoot 'models\hud_objects') -File | Where-Object Extension -In '.dae','.mat','.hud')
foreach ($entry in $heldFiles) {
    $raw = Get-Content -Raw -LiteralPath $entry.FullName
    foreach ($match in [regex]::Matches($raw, '(?i)([a-z0-9_-]{1,128})\.(jpg|tga|png|dds|bmp)')) { $protected[$match.Groups[1].Value.ToLowerInvariant()] = $true }
}
$auditCount=0
$rows = @(foreach ($file in Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName) {
    $auditCount++
    if (($auditCount % 200) -eq 0) { Write-Host "Auditing pack file $auditCount..." }
    $relative = $file.FullName.Substring($source.Length+1).Replace('\','/')
    $key = $file.BaseName.ToLowerInvariant()
    $original = Get-TextureBaselinePath $game $baseline $relative
    $row = [ordered]@{
        Path=$relative; Bytes=$file.Length; Width=0; Height=0; Bpp=0
        OriginalWidth=0; OriginalHeight=0; OriginalBpp=0
        EstimatedRGBABytes=0L; EstimatedExtraRGBABytes=0L
        Baseline=$(if ($baseline.ContainsKey($relative)) { 'verified-installer-backup' } else { 'installed-original' })
        ReviewedMaterials=@(); Status='excluded'; Reason='Not an image replacement'
    }
    if ($file.Extension -in '.jpg','.tga','.png','.bmp','.gif') {
        $replacement = Get-TextureImageInfo $file.FullName
        $row.Width=$replacement.Width; $row.Height=$replacement.Height; $row.Bpp=$replacement.Bpp
        if (Test-Path -LiteralPath $original) {
            $stock = Get-TextureImageInfo $original
            $row.OriginalWidth=$stock.Width; $row.OriginalHeight=$stock.Height; $row.OriginalBpp=$stock.Bpp
            $row.EstimatedRGBABytes=Get-TextureRGBABytes $replacement.Width $replacement.Height
            $row.EstimatedExtraRGBABytes=$row.EstimatedRGBABytes-(Get-TextureRGBABytes $stock.Width $stock.Height)
        }
        $materialUses = @()
        if ($uses.ContainsKey($key)) { $materialUses=@($uses[$key]) }
        $row.ReviewedMaterials=@($materialUses | ForEach-Object Material | Sort-Object -Unique)
        if ($relative -match '^(models/(hud_objects|player|items)|textures/(items|lightfx|signs|lang_specific|decals[^/]*))/' -or $protected.ContainsKey($key)) {
            $row.Reason='Protected VR, held item, decal, light effect or readable information'
        } elseif (-not (Test-Path -LiteralPath $original)) { $row.Reason='No identical original path'
        } elseif (-not $nameCounts.ContainsKey($key) -or $nameCounts[$key] -ne 1) { $row.Reason='Ambiguous resource basename'
        } elseif ($materialUses.Count -eq 0) { $row.Reason='No indexed material use'
        } elseif (@($materialUses | Where-Object { -not $_.Safe }).Count -gt 0) { $row.Reason='Non-opaque, non-diffuse, animated or special material use'
        } elseif ($relative -match '(?i)(sign|poster|noteboard|papers|symbol|keypad|numpad|code|map_)[^/]*\.(jpg|tga)$') { $row.Reason='Readable information or puzzle artwork'
        } elseif ($replacement.Bpp -ne 24 -or $stock.Bpp -ne 24) { $row.Reason='Alpha or non-RGB pixel format'
        } elseif ($replacement.Width*$stock.Height -ne $replacement.Height*$stock.Width) { $row.Reason='Changed aspect ratio'
        } elseif ($replacement.Width -le $stock.Width -or $replacement.Height -le $stock.Height) { $row.Reason='No resolution increase'
        } elseif ($replacement.Width -gt $stock.Width*4 -or $replacement.Height -gt $stock.Height*4) { $row.Reason='Over 4x dimension increase'
        } elseif (($replacement.Width -band ($replacement.Width-1)) -ne 0 -or ($replacement.Height -band ($replacement.Height-1)) -ne 0) { $row.Reason='Non-power-of-two dimensions'
        } elseif ($file.Extension -ne '.jpg') { $row.Reason='Non-JPEG diffuse needs separate decoder/visual review'
        } else { $row.Status='candidate'; $row.Reason='Eligible opaque diffuse; pending visual and memory review' }
    }
    [pscustomobject]$row
})
$outputRoot = Join-Path $repoRoot 'build\texture-audit'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$rows | ConvertTo-Json -Depth 6 | Set-Content -LiteralPath (Join-Path $outputRoot 'inventory.json') -Encoding UTF8
$rows | Group-Object Reason | Select-Object Name,Count | Format-Table -AutoSize
$rows | Where-Object Status -EQ 'candidate' | Group-Object Width,Height |
    ForEach-Object { [pscustomobject]@{Size=$_.Name; Count=$_.Count; ExtraMiB=($_.Group | Measure-Object EstimatedExtraRGBABytes -Sum).Sum/1MB} } | Format-Table -AutoSize
Write-Host "Audited $($rows.Count) pack files. Inventory: $outputRoot\inventory.json"
