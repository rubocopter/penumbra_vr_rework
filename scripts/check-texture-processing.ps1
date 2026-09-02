[CmdletBinding()]
param([Parameter(Mandatory=$true)][string]$SourceRoot)
# Compare sampled output pixels against the uncompressed resized reference.
# PSNR is a compression check, not a perceptual or in-headset quality guarantee.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'texture-pack-common.ps1')
$repoRoot=Split-Path -Parent $PSScriptRoot
$manifest=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') | ConvertFrom-Json
$results=@()
foreach($file in $manifest.Files | Where-Object Processing -EQ 'bicubic-jpeg95') {
    $sourcePath=Join-Path $SourceRoot $file.Path
    if ((Get-FileHash -LiteralPath $sourcePath).Hash -ne $file.SourceSHA256) { throw 'Changed processing source.' }
    $source=[Drawing.Image]::FromFile($sourcePath)
    $reference=New-Object Drawing.Bitmap($file.Width,$file.Height,[Drawing.Imaging.PixelFormat]::Format24bppRgb)
    $output=[Drawing.Bitmap]::FromFile((Join-Path $repoRoot ('data/'+$file.Path)))
    $graphics=[Drawing.Graphics]::FromImage($reference)
    $attributes=New-Object Drawing.Imaging.ImageAttributes
    try {
        $graphics.InterpolationMode=[Drawing.Drawing2D.InterpolationMode]::HighQualityBicubic
        $graphics.PixelOffsetMode=[Drawing.Drawing2D.PixelOffsetMode]::HighQuality
        $attributes.SetWrapMode([Drawing.Drawing2D.WrapMode]::TileFlipXY)
        $rect=New-Object Drawing.Rectangle(0,0,$file.Width,$file.Height)
        $graphics.DrawImage($source,$rect,0,0,$source.Width,$source.Height,[Drawing.GraphicsUnit]::Pixel,$attributes)
        $step=[Math]::Max(1,[int][Math]::Floor([Math]::Max($file.Width,$file.Height)/64))
        $sumSquared=0.0; $sumAbsolute=0.0; $samples=0
        for($y=0;$y -lt $file.Height;$y+=$step) { for($x=0;$x -lt $file.Width;$x+=$step) {
            $a=$reference.GetPixel($x,$y); $b=$output.GetPixel($x,$y)
            foreach($delta in @(([double]$a.R-$b.R),([double]$a.G-$b.G),([double]$a.B-$b.B))) {
                $sumSquared+=$delta*$delta; $sumAbsolute+=[Math]::Abs($delta); $samples++
            }
        } }
        $psnr=99.0
        if ($sumSquared -gt 0) { $psnr=10*[Math]::Log10(255.0*255/($sumSquared/$samples)) }
        $meanError=$sumAbsolute/$samples
        if ($psnr -lt 35 -or $meanError -gt 3) { throw "Excessive JPEG error in $($file.Path): PSNR $psnr, mean error $meanError" }
        $results += [pscustomobject]@{Path=$file.Path; PSNR=[Math]::Round($psnr,3); MeanRGBError255=[Math]::Round($meanError,4); Samples=$samples}
    } finally { $attributes.Dispose(); $graphics.Dispose(); $source.Dispose(); $reference.Dispose(); $output.Dispose() }
}
$outputPath=Join-Path $repoRoot 'build\texture-audit\compression-check.json'
$results | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath $outputPath -Encoding UTF8
$minimum=($results | Measure-Object PSNR -Minimum).Minimum
$maximum=($results | Measure-Object MeanRGBError255 -Maximum).Maximum
Write-Host "JPEG processing check: $($results.Count) images, minimum sampled PSNR $minimum dB, maximum mean RGB error $maximum / 255."
