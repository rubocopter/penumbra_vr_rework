[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot,
    # Use only immediately after an audit/review of these exact inputs.
    [switch]$UseCurrentAudit
)
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'texture-pack-common.ps1')
$repoRoot=Split-Path -Parent $PSScriptRoot
$source=(Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$game=(Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$baseline=Get-TextureBaselineIndex $game
$policyPath=Join-Path $repoRoot 'docs\TEXTURE_POLICY.json'
$policy=Get-Content -Raw -LiteralPath $policyPath | ConvertFrom-Json
$manifestPath=Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json'
$previous=Get-Content -Raw -LiteralPath $manifestPath | ConvertFrom-Json
$previousByPath=@{}
foreach($entry in $previous.Files) { $previousByPath[$entry.Path]=$entry }
if (-not $UseCurrentAudit) {
    & (Join-Path $PSScriptRoot 'audit-texture-pack.ps1') -SourceRoot $source -GameRoot $game
    & (Join-Path $PSScriptRoot 'review-texture-candidates.ps1') -SourceRoot $source -GameRoot $game
    throw 'Audit and review sheets regenerated. Inspect them, then rerun with -UseCurrentAudit.'
}
$inventory=@(Get-Content -Raw (Join-Path $repoRoot 'build\texture-audit\inventory.json') | ConvertFrom-Json)
$metricsByPath=@{}
foreach($metric in Get-Content -Raw (Join-Path $repoRoot 'build\texture-audit\visual-metrics.json') | ConvertFrom-Json) { $metricsByPath[$metric.Path]=$metric }
$exclusions=@{}
foreach($property in $policy.ExcludedAfterVisualReview.PSObject.Properties) { $exclusions[$property.Name]=[string]$property.Value }
$records=@(); $decisions=@(); $extraTotal=0L
foreach($row in $inventory) {
    $reason=$row.Reason; $selected=$false
    $width=0; $height=0; $mode=''
    if ($row.Status -eq 'candidate') {
        if (-not $metricsByPath.ContainsKey($row.Path)) { throw "Missing visual review: $($row.Path)" }
        $metric=$metricsByPath[$row.Path]
        $retain=$row.Path -in $policy.RetainSourcePaths
        $scale=[Math]::Min(1.0,[Math]::Min([double]$policy.StockScale*$row.OriginalWidth/$row.Width,[double]$policy.MaxDimension/[Math]::Max($row.Width,$row.Height)))
        $width=[int]($row.Width*$scale); $height=[int]($row.Height*$scale)
        if ($retain) { $width=$row.Width; $height=$row.Height }
        if ($exclusions.ContainsKey($row.Path)) { $reason=$exclusions[$row.Path]
        } elseif ($width -le $row.OriginalWidth -or $height -le $row.OriginalHeight) { $reason='No resolution gain under the 1024 cap; retain original'
        } elseif ($metric.Correlation -lt $policy.MinimumCorrelation -or $metric.MeanRGBError -gt $policy.MaximumMeanRGBError -or $metric.StockLumaDeviation -lt $policy.MinimumLumaDeviation) { $reason='Low-contrast or changed-pattern visual review; retain original'
        } else { $selected=$true; $reason='Reviewed opaque diffuse, bounded resolution, original layout retained' }
        if ($retain -and -not $selected) { throw "Previously tested texture unexpectedly excluded: $($row.Path)" }
    }
    $decisions += [pscustomobject]@{Path=$row.Path; Selected=$selected; Reason=$reason; SourceWidth=$row.Width; SourceHeight=$row.Height}
    if (-not $selected) { continue }
    if ($row.Path -notmatch '^(models|textures)/([a-z0-9_]+/)+[a-z0-9_]+\.jpg$') { throw 'Invalid texture path.' }
    $sourcePath=Join-Path $source $row.Path
    $stockPath=Get-TextureBaselinePath $game $baseline $row.Path
    $sourceInfo=Get-TextureImageInfo $sourcePath
    $stockInfo=Get-TextureImageInfo $stockPath
    if ($sourceInfo.Width -ne $row.Width -or $sourceInfo.Height -ne $row.Height -or $stockInfo.Width -ne $row.OriginalWidth -or $stockInfo.Height -ne $row.OriginalHeight) { throw 'Stale texture audit dimensions.' }
    $sourceHash=(Get-FileHash -LiteralPath $sourcePath).Hash
    $stockHash=(Get-FileHash -LiteralPath $stockPath).Hash
    if ($metric.SourceSHA256 -ne $sourceHash -or $metric.StockSHA256 -ne $stockHash) { throw "Stale visual review: $($row.Path)" }
    $extra=(Get-TextureRGBABytes $width $height)-(Get-TextureRGBABytes $stockInfo.Width $stockInfo.Height)
    $extraTotal+=$extra
    $records += [pscustomobject][ordered]@{
        Path=$row.Path; Width=$width; Height=$height; StockWidth=$stockInfo.Width; StockHeight=$stockInfo.Height
        SourceWidth=$row.Width; SourceHeight=$row.Height; SourceBytes=$row.Bytes; SourceSHA256=$sourceHash
        StockSHA256=$stockHash; EstimatedRGBABytes=(Get-TextureRGBABytes $width $height); EstimatedExtraRGBABytes=$extra
        Processing=$(if($retain){'unchanged-source'}else{'bicubic-jpeg95'}); Bytes=0L; SHA256=''
        ReviewedMaterials=$row.ReviewedMaterials
        SourceCorrelation=$metric.Correlation; SourceMeanRGBError=$metric.MeanRGBError
    }
}
if ($records.Count -lt 100 -or $extraTotal -gt $policy.ExtraRGBABytesBudget) { throw 'Unexpected selection size or memory budget exceeded.' }
# Validate all destinations before generating anything. Preserve arbitrary user
# edits; only known previous pack versions can be replaced by this importer.
foreach($record in $records) {
    $dest=Join-Path $repoRoot ('data/'+$record.Path)
    if (Test-Path -LiteralPath $dest) {
        $hash=(Get-FileHash -LiteralPath $dest).Hash
        if (-not $previousByPath.ContainsKey($record.Path) -or $previousByPath[$record.Path].SHA256 -ne $hash) { throw "Unowned or modified destination: $dest" }
    }
}
$stage=Join-Path $repoRoot ('build\texture-import-'+[Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $stage -Force | Out-Null
$jpeg=[Drawing.Imaging.ImageCodecInfo]::GetImageEncoders() | Where-Object MimeType -EQ 'image/jpeg'
$encoderParameters=New-Object Drawing.Imaging.EncoderParameters(1)
$encoderParameters.Param[0]=New-Object Drawing.Imaging.EncoderParameter([Drawing.Imaging.Encoder]::Quality,[long]$policy.JpegQuality)
try {
    foreach($record in $records) {
        $sourcePath=Join-Path $source $record.Path
        $dest=Join-Path $stage $record.Path
        New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
        if ($record.Processing -eq 'unchanged-source') { Copy-Item -LiteralPath $sourcePath -Destination $dest }
        else {
            $sourceImage=[Drawing.Image]::FromFile($sourcePath)
            $bitmap=New-Object Drawing.Bitmap($record.Width,$record.Height,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
            $graphics=[Drawing.Graphics]::FromImage($bitmap)
            $attributes=New-Object Drawing.Imaging.ImageAttributes
            try {
                $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
                $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
                $attributes.SetWrapMode([Drawing.Drawing2D.WrapMode]::TileFlipXY)
                $rect=New-Object Drawing.Rectangle(0,0,$record.Width,$record.Height)
                $graphics.DrawImage($sourceImage,$rect,0,0,$sourceImage.Width,$sourceImage.Height,[Drawing.GraphicsUnit]::Pixel,$attributes)
                $bitmap.Save($dest,$jpeg,$encoderParameters)
            } finally { $attributes.Dispose(); $graphics.Dispose(); $bitmap.Dispose(); $sourceImage.Dispose() }
        }
        $record.Bytes=(Get-Item -LiteralPath $dest).Length
        $record.SHA256=(Get-FileHash -LiteralPath $dest).Hash
        $outputInfo=Get-TextureImageInfo $dest
        if ($outputInfo.Width -ne $record.Width -or $outputInfo.Height -ne $record.Height -or $outputInfo.Bpp -ne 24) { throw 'Generated texture format mismatch.' }
    }
} finally { $encoderParameters.Dispose() }
# Entire batch generated/decoded successfully before replacing project assets.
foreach($record in $records) {
    $dest=Join-Path $repoRoot ('data/'+$record.Path)
    New-Item -ItemType Directory -Path (Split-Path -Parent $dest) -Force | Out-Null
    Copy-Item -LiteralPath (Join-Path $stage $record.Path) -Destination $dest
}
[ordered]@{
    Version=2; SourceName=$previous.SourceName; SourceURL=$previous.SourceURL; Creator=$previous.Creator
    Uploader=$previous.Uploader; OriginalAssetCredit=$previous.OriginalAssetCredit; PermissionsChecked=$previous.PermissionsChecked
    Distribution=$previous.Distribution; PolicySHA256=(Get-TexturePolicyHash $policyPath)
    MaxDimension=$policy.MaxDimension; ExtraRGBABytesBudget=$policy.ExtraRGBABytesBudget
    EstimatedExtraRGBABytes=$extraTotal; Files=$records
} | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath $manifestPath -Encoding UTF8
[ordered]@{
    SourceFileCount=$inventory.Count; CandidateCount=@($inventory | Where-Object Status -EQ 'candidate').Count
    SelectedCount=$records.Count; Scope='All supplied files; conservative opaque diffuse-only selection'
    Files=$decisions
} | ConvertTo-Json -Depth 5 | Set-Content -LiteralPath (Join-Path $repoRoot 'docs\TEXTURE_AUDIT.json') -Encoding UTF8
Write-Host "Imported $($records.Count) reviewed textures; $($extraTotal/1MB) MiB estimated extra RGBA+mips. Stage retained: $stage"
