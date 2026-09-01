[CmdletBinding()]
param([string]$ContentRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
$repoRoot=Split-Path -Parent $PSScriptRoot
if (-not $ContentRoot) { $ContentRoot=Join-Path $repoRoot 'data' }
$manifest=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') | ConvertFrom-Json
Add-Type -AssemblyName System.Drawing
if ($manifest.Version -ne 1 -or $manifest.Files.Count -ne 9) { throw 'Unexpected texture selection version/count.' }
if ($manifest.Creator -ne 'Bret2011' -or $manifest.OriginalAssetCredit -ne 'Frictional Games' -or
    $manifest.SourceURL -ne 'https://www.nexusmods.com/penumbraoverture/mods/5?tab=description') {
    throw 'Texture source attribution is missing or changed.'
}
$credits=Get-Content -Raw -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_CREDITS.md')
foreach ($credit in @('Bret2011','c3pohumancyborgrelations','Frictional Games')) {
    if (-not $credits.Contains($credit)) { throw "Missing texture credit: $credit" }
}
$sum=0L
$seen=@{}
foreach ($file in $manifest.Files) {
    if ($file.Path -notmatch '^textures/(iron_mine|locker_room|modern_mine|new_storage_room)/[a-z0-9_]+\.jpg$' -or
        $seen.ContainsKey($file.Path)) { throw "Invalid or duplicate selected path: $($file.Path)" }
    $seen[$file.Path]=$true
    $path=Join-Path $ContentRoot $file.Path
    if ((Get-FileHash -LiteralPath $path).Hash -ne $file.SHA256) { throw "Texture hash mismatch: $path" }
    $bitmap=[Drawing.Image]::FromFile($path)
    try {
        if ($bitmap.Width -ne $file.Width -or $bitmap.Height -ne $file.Height -or
            $bitmap.Width -gt 1024 -or $bitmap.Height -gt 1024 -or
            [Drawing.Image]::GetPixelFormatSize($bitmap.PixelFormat) -ne 24 -or
            $bitmap.Width*$file.StockHeight -ne $bitmap.Height*$file.StockWidth) { throw "Texture format mismatch: $path" }
    } finally { $bitmap.Dispose() }
    $extra=[long][Math]::Ceiling(($file.Width*$file.Height-$file.StockWidth*$file.StockHeight)*4.0*4.0/3.0)
    if ($extra -le 0 -or $extra -ne $file.EstimatedExtraRGBABytes -or $file.ReviewedMaterials.Count -eq 0) { throw "Invalid texture budget/review: $path" }
    $sum+=$extra
}
if ($sum -ne $manifest.EstimatedExtraRGBABytes -or $sum -gt 40MB) { throw 'Texture memory budget exceeded or incorrect.' }
$actual=@(Get-ChildItem -LiteralPath (Join-Path $ContentRoot 'textures') -Recurse -File -Filter '*.jpg')
if ($actual.Count -ne $manifest.Files.Count) { throw 'Unexpected JPEGs outside the reviewed texture allowlist.' }
Write-Host "Texture selection OK: 9 opaque diffuse JPEGs, <=1024, $($sum/1MB) MiB estimated extra RGBA+mips."
