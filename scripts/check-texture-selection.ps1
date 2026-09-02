[CmdletBinding()]
param([string]$ContentRoot)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'texture-pack-common.ps1')
$repoRoot=Split-Path -Parent $PSScriptRoot
if (-not $ContentRoot) { $ContentRoot=Join-Path $repoRoot 'data' }
$ContentRoot=(Resolve-Path -LiteralPath $ContentRoot).Path.TrimEnd('\')
$manifest=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') | ConvertFrom-Json
$policyPath=Join-Path $repoRoot 'docs\TEXTURE_POLICY.json'
$policy=Get-Content -Raw -LiteralPath $policyPath | ConvertFrom-Json
$audit=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_AUDIT.json') | ConvertFrom-Json
if ($manifest.Version -ne 2 -or $manifest.Files.Count -ne $audit.SelectedCount -or $manifest.Files.Count -lt 100 -or
    $audit.Files.Count -ne $audit.SourceFileCount -or $manifest.PolicySHA256 -ne (Get-TexturePolicyHash $policyPath)) { throw 'Invalid selection/audit/policy version or count.' }
if ($manifest.Creator -ne 'Bret2011' -or $manifest.OriginalAssetCredit -ne 'Frictional Games' -or
    $manifest.SourceURL -ne 'https://www.nexusmods.com/penumbraoverture/mods/5?tab=description') { throw 'Missing texture attribution.' }
$credits=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_CREDITS.md')
foreach($credit in @('Bret2011','c3pohumancyborgrelations','Frictional Games')) {
    if (-not $credits.Contains($credit)) { throw "Missing credit: $credit" }
}
$seen=@{}; $sum=0L; $disk=0L
foreach($file in $manifest.Files) {
    if ($file.Path -notmatch '^(models|textures)/([a-z0-9_]+/)+[a-z0-9_]+\.jpg$' -or
        $file.Path -match '^(models/(hud_objects|player|items)|textures/(items|lightfx|signs|lang_specific|decals[^/]*))/' -or
        $seen.ContainsKey($file.Path)) { throw "Invalid/protected/duplicate texture path: $($file.Path)" }
    $seen[$file.Path]=$true
    $path=Join-Path $ContentRoot $file.Path
    if ((Get-FileHash -LiteralPath $path).Hash -ne $file.SHA256 -or (Get-Item -LiteralPath $path).Length -ne $file.Bytes) { throw "Texture hash/size mismatch: $path" }
    $info=Get-TextureImageInfo $path
    if ($info.Width -ne $file.Width -or $info.Height -ne $file.Height -or $info.Bpp -ne 24 -or
        $info.Width -gt $policy.MaxDimension -or $info.Height -gt $policy.MaxDimension -or
        $info.Width*$file.StockHeight -ne $info.Height*$file.StockWidth -or
        ($info.Width -band ($info.Width-1)) -ne 0 -or ($info.Height -band ($info.Height-1)) -ne 0) { throw "Invalid texture format: $path" }
    $retain=$file.Path -in $policy.RetainSourcePaths
    if ($retain) {
        if ($file.Processing -ne 'unchanged-source' -or $file.SHA256 -ne $file.SourceSHA256) { throw 'Previously tested texture changed.' }
    } elseif ($file.Processing -ne 'bicubic-jpeg95' -or $file.Width -gt $policy.StockScale*$file.StockWidth -or $file.Height -gt $policy.StockScale*$file.StockHeight) {
        throw 'Unexpected processing or resolution multiplier.'
    }
    if ($file.SourceCorrelation -lt $policy.MinimumCorrelation -or $file.SourceMeanRGBError -gt $policy.MaximumMeanRGBError -or
        $file.SourceSHA256 -notmatch '^[A-Fa-f0-9]{64}$' -or $file.StockSHA256 -notmatch '^[A-Fa-f0-9]{64}$' -or $file.ReviewedMaterials.Count -eq 0) { throw 'Missing source/material/visual evidence.' }
    $bytes=Get-TextureRGBABytes $file.Width $file.Height
    $extra=$bytes-(Get-TextureRGBABytes $file.StockWidth $file.StockHeight)
    if ($extra -le 0 -or $extra -ne $file.EstimatedExtraRGBABytes -or $bytes -ne $file.EstimatedRGBABytes) { throw 'Invalid memory estimate.' }
    $sum+=$extra; $disk+=$file.Bytes
}
foreach($decision in $audit.Files) {
    if ([bool]$decision.Selected -ne $seen.ContainsKey($decision.Path)) { throw "Audit/selection mismatch: $($decision.Path)" }
}
if ($sum -ne $manifest.EstimatedExtraRGBABytes -or $sum -gt $policy.ExtraRGBABytesBudget -or $manifest.ExtraRGBABytesBudget -ne $policy.ExtraRGBABytesBudget) { throw 'Memory budget exceeded or incorrect.' }
$actual=@(foreach($directory in @('models','textures')) {
    Get-ChildItem -LiteralPath (Join-Path $ContentRoot $directory) -Recurse -File -Filter '*.jpg' | Where-Object {
        $_.FullName.Substring($ContentRoot.TrimEnd('\').Length+1).Replace('\','/') -notmatch '^models/(hud_objects|items|player)/'
    }
})
if ($actual.Count -ne $seen.Count) { throw 'Unreviewed JPEGs outside the texture allowlist.' }
Write-Host ("Texture selection OK: {0} opaque diffuse images, <=1024, {1:N2} MiB on disk, {2:N2} MiB estimated extra RGBA+mips." -f $seen.Count,($disk/1MB),($sum/1MB))
