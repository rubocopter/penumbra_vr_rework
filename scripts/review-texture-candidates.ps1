[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot,
    [int]$MaxDimension = 4096,
    [switch]$OnlyUnreviewed
)
# Diagnostic resampling only: creates comparison sheets, never edits textures.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'texture-pack-common.ps1')
$repoRoot=Split-Path -Parent $PSScriptRoot
$outputRoot=Join-Path $repoRoot 'build\texture-audit'
$source=(Resolve-Path -LiteralPath $SourceRoot).Path
$game=(Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$baseline=Get-TextureBaselineIndex $game
$rows=@(Get-Content -Raw (Join-Path $outputRoot 'inventory.json') | ConvertFrom-Json |
    Where-Object { $_.Status -eq 'candidate' -and $_.Width -le $MaxDimension -and $_.Height -le $MaxDimension })
$previousMetrics=@()
$sheetPrefix='review'
if ($OnlyUnreviewed) {
    $previousMetrics=@(Get-Content -Raw (Join-Path $outputRoot 'visual-metrics.json') | ConvertFrom-Json)
    $reviewedPaths=@{}
    foreach($metric in $previousMetrics) { $reviewedPaths[$metric.Path]=$true }
    $rows=@($rows | Where-Object { -not $reviewedPaths.ContainsKey($_.Path) })
    $sheetPrefix='review-extra'
}
if ($rows.Count -eq 0) { Write-Host 'No unreviewed candidates.'; return }
function Get-Preview([string]$path, [int]$width, [int]$height) {
    $sourceImage=[Drawing.Image]::FromFile($path)
    $preview=New-Object Drawing.Bitmap($width,$height)
    $graphics=[Drawing.Graphics]::FromImage($preview)
    try {
        $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $graphics.DrawImage($sourceImage,0,0,$width,$height)
    } finally { $graphics.Dispose(); $sourceImage.Dispose() }
    return ,$preview
}
$metrics=@()
$sheet=$null; $canvas=$null
$font=New-Object Drawing.Font('Consolas',9)
try {
    for($index=0; $index -lt $rows.Count; $index++) {
        $row=$rows[$index]
        if (($index % 12) -eq 0) {
            if ($null -ne $sheet) { $sheet.Save((Join-Path $outputRoot ('{0}-{1:d2}.png' -f $sheetPrefix,[int][Math]::Floor(($index-1)/12)))); $canvas.Dispose(); $sheet.Dispose() }
            $sheet=New-Object Drawing.Bitmap(1536,1128)
            $canvas=[Drawing.Graphics]::FromImage($sheet)
            $canvas.Clear([Drawing.Color]::FromArgb(28,28,28))
        }
        $stockPath=Get-TextureBaselinePath $game $baseline $row.Path
        $newPath=Join-Path $source $row.Path
        $stock=Get-Preview $stockPath 32 32
        $replacement=Get-Preview $newPath 32 32
        $sumA=0.0; $sumB=0.0; $sumAA=0.0; $sumBB=0.0; $sumAB=0.0; $absError=0.0
        try {
            for($y=0;$y -lt 32;$y++) { for($x=0;$x -lt 32;$x++) {
                $a=$stock.GetPixel($x,$y); $b=$replacement.GetPixel($x,$y)
                $la=($a.R*.2126+$a.G*.7152+$a.B*.0722)/255
                $lb=($b.R*.2126+$b.G*.7152+$b.B*.0722)/255
                $sumA+=$la; $sumB+=$lb; $sumAA+=$la*$la; $sumBB+=$lb*$lb; $sumAB+=$la*$lb
                $absError+=([Math]::Abs([int]$a.R-$b.R)+[Math]::Abs([int]$a.G-$b.G)+[Math]::Abs([int]$a.B-$b.B))/(3.0*255)
            } }
        } finally { $stock.Dispose(); $replacement.Dispose() }
        $varianceA=[Math]::Max([double]0,$sumAA/1024-($sumA/1024)*($sumA/1024))
        $varianceB=[Math]::Max([double]0,$sumBB/1024-($sumB/1024)*($sumB/1024))
        $correlation=0.0
        if ($varianceA*$varianceB -gt 1e-12) { $correlation=($sumAB/1024-($sumA/1024)*($sumB/1024))/[Math]::Sqrt($varianceA*$varianceB) }
        $metrics += [pscustomobject]@{
            Path=$row.Path; Sheet=[int][Math]::Floor($index/12); Cell=$index%12
            SheetPrefix=$sheetPrefix
            SourceSHA256=(Get-FileHash -LiteralPath $newPath).Hash
            StockSHA256=(Get-FileHash -LiteralPath $stockPath).Hash
            Correlation=[Math]::Round($correlation,5); MeanRGBError=[Math]::Round($absError/1024,5)
            StockLumaDeviation=[Math]::Round([Math]::Sqrt($varianceA),5)
        }
        $cellX=($index%3)*512; $cellY=([int][Math]::Floor(($index%12)/3))*282
        $label=($row.Path -replace '/',' / ')
        if ($label.Length -gt 65) { $label=$row.Path }
        $canvas.DrawString($label,$font,[Drawing.Brushes]::White,$cellX+4,$cellY+2)
        $canvas.DrawString(('Original {0}x{1}       Pack {2}x{3}   r={4:0.000}' -f $row.OriginalWidth,$row.OriginalHeight,$row.Width,$row.Height,$correlation),$font,[Drawing.Brushes]::LightGray,$cellX+4,$cellY+18)
        $previewWidth=[int][Math]::Min(244,244*$row.Width/$row.Height)
        $previewHeight=[int][Math]::Min(244,244*$row.Height/$row.Width)
        $left=Get-Preview $stockPath $previewWidth $previewHeight
        $right=Get-Preview $newPath $previewWidth $previewHeight
        try {
            $canvas.DrawImageUnscaled($left,$cellX+4,$cellY+36)
            $canvas.DrawImageUnscaled($right,$cellX+260,$cellY+36)
        } finally { $left.Dispose(); $right.Dispose() }
        if (($index % 12) -eq 11) { Write-Host "Reviewed $($index+1)/$($rows.Count) texture pairs." }
    }
    if ($null -ne $sheet) { $sheet.Save((Join-Path $outputRoot ('{0}-{1:d2}.png' -f $sheetPrefix,[int][Math]::Floor(($rows.Count-1)/12)))) }
} finally {
    if ($null -ne $canvas) { $canvas.Dispose() }
    if ($null -ne $sheet) { $sheet.Dispose() }
    $font.Dispose()
}
$allMetrics=@($previousMetrics)+@($metrics)
$allMetrics | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'visual-metrics.json') -Encoding UTF8
if (@($metrics | Where-Object StockLumaDeviation -GT 0.02).Count -eq 0) { throw 'Invalid review metrics: no non-flat reference images.' }
Write-Host "Generated comparison sheets and metrics for $($rows.Count) candidates. Metrics check alignment/colour, not subjective improvement."
