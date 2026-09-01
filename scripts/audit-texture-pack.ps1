[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)][string]$SourceRoot,
    [Parameter(Mandatory=$true)][string]$GameRoot
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Drawing
function Get-ImageInfo([string]$path) {
    if ([IO.Path]::GetExtension($path) -eq '.tga') {
        $stream = [IO.File]::OpenRead($path)
        try {
            $header = New-Object byte[] 18
            if ($stream.Read($header,0,18) -ne 18) { throw 'Truncated TGA' }
            return [pscustomobject]@{Width=[BitConverter]::ToUInt16($header,12);Height=[BitConverter]::ToUInt16($header,14);Bpp=[int]$header[16]}
        } finally { $stream.Dispose() }
    }
    $bitmap = [Drawing.Image]::FromFile($path)
    try { return [pscustomobject]@{Width=$bitmap.Width;Height=$bitmap.Height;Bpp=[Drawing.Image]::GetPixelFormatSize($bitmap.PixelFormat)} }
    finally { $bitmap.Dispose() }
}
$source = (Resolve-Path -LiteralPath $SourceRoot).Path.TrimEnd('\')
$game = (Resolve-Path -LiteralPath $GameRoot).Path.TrimEnd('\')
$gameImages = @(Get-ChildItem -LiteralPath $game -Recurse -File | Where-Object Extension -In '.jpg','.tga','.png','.dds','.bmp')
$nameCounts = @{}
foreach ($entry in $gameImages) {
    $key = $entry.BaseName.ToLowerInvariant()
    if (-not $nameCounts.ContainsKey($key)) { $nameCounts[$key] = 0 }
    $nameCounts[$key]++
}
$rows = foreach ($file in (Get-ChildItem -LiteralPath $source -Recurse -File | Sort-Object FullName)) {
    $relative = $file.FullName.Substring($source.Length+1).Replace('\','/')
    $original = Join-Path $game $relative
    $row = [ordered]@{Path=$relative;Bytes=$file.Length;Width=0;Height=0;OriginalWidth=0;OriginalHeight=0;EstimatedExtraRGBABytes=0;Status='excluded';Reason='Not a world diffuse JPEG'}
    if ($relative -match '^textures/(mine|iron_mine|modern_mine|old_storage|new_storage_room|base_entrance|locker_room|mining_room|refinery)/' -and
        $file.Extension -eq '.jpg' -and $file.BaseName -notmatch 'bump|normal|spec|illum|alpha|shadow|decal|sign|light|flare|glow|window|grate|wire|ladder') {
        if (-not (Test-Path -LiteralPath $original)) { $row.Reason='No identical original path' }
        elseif ($nameCounts[$file.BaseName.ToLowerInvariant()] -ne 1) { $row.Reason='Ambiguous resource basename' }
        else {
            $replacement = Get-ImageInfo $file.FullName
            $stock = Get-ImageInfo $original
            $row.Width=$replacement.Width; $row.Height=$replacement.Height
            $row.OriginalWidth=$stock.Width; $row.OriginalHeight=$stock.Height
            # Conservative uncompressed RGBA + full mip-chain estimate; not JPEG disk size.
            $row.EstimatedExtraRGBABytes=[long][Math]::Ceiling([Math]::Max(0,($replacement.Width*$replacement.Height-$stock.Width*$stock.Height))*4.0*4.0/3.0)
            if ($replacement.Width*$stock.Height -ne $replacement.Height*$stock.Width) { $row.Reason='Changed aspect ratio' }
            elseif ($replacement.Width -gt 1024 -or $replacement.Height -gt 1024) { $row.Reason='Over 1024 dimension cap' }
            elseif ($replacement.Width -le $stock.Width -or $replacement.Height -le $stock.Height) { $row.Reason='No resolution increase' }
            elseif ($replacement.Width -gt $stock.Width*4 -or $replacement.Height -gt $stock.Height*4) { $row.Reason='Over 4x dimension increase' }
            elseif ($replacement.Bpp -ne 24 -or $stock.Bpp -ne 24) { $row.Reason='Unexpected pixel format' }
            elseif ((Get-FileHash -LiteralPath $file.FullName).Hash -eq (Get-FileHash -LiteralPath $original).Hash) { $row.Reason='Identical content' }
            else { $row.Status='candidate'; $row.Reason='Needs material and visual review' }
        }
    }
    [pscustomobject]$row
}
$outputRoot = Join-Path (Split-Path -Parent $PSScriptRoot) 'build\texture-audit'
New-Item -ItemType Directory -Path $outputRoot -Force | Out-Null
$rows | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $outputRoot 'inventory.json') -Encoding UTF8
$rows | Group-Object Reason | Select-Object Name,Count | Format-Table -AutoSize
$rows | Where-Object Status -eq 'candidate' | ConvertTo-Json -Depth 3
