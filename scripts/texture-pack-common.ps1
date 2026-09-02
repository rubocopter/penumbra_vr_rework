# Read-only helpers: installed replacements are never the stock baseline.
Set-StrictMode -Version Latest
Add-Type -AssemblyName System.Drawing

function Get-TexturePolicyHash([string]$Path) {
    # Git can check text out as LF or CRLF. Bind to the policy content, not the
    # host's newline convention or UTF-8 BOM, so CI and local imports agree.
    $text=[IO.File]::ReadAllText($Path).Replace("`r`n","`n")
    $sha=[Security.Cryptography.SHA256]::Create()
    try { return [BitConverter]::ToString($sha.ComputeHash([Text.Encoding]::UTF8.GetBytes($text))).Replace('-','') }
    finally { $sha.Dispose() }
}

function Get-TextureImageInfo([string]$Path) {
    if ([IO.Path]::GetExtension($Path) -ieq '.tga') {
        $stream = [IO.File]::OpenRead($Path)
        try {
            $header = New-Object byte[] 18
            if ($stream.Read($header, 0, 18) -ne 18) { throw "Truncated TGA: $Path" }
            return [pscustomobject]@{Width=[BitConverter]::ToUInt16($header,12); Height=[BitConverter]::ToUInt16($header,14); Bpp=[int]$header[16]}
        } finally { $stream.Dispose() }
    }
    $bitmap = [Drawing.Image]::FromFile($Path)
    try { return [pscustomobject]@{Width=$bitmap.Width; Height=$bitmap.Height; Bpp=[Drawing.Image]::GetPixelFormatSize($bitmap.PixelFormat)} }
    finally { $bitmap.Dispose() }
}

function Get-TextureBaselineIndex([string]$GameRoot) {
    $index = @{}
    $stateRoot = Join-Path (Split-Path -Parent $GameRoot) '.penumbravr'
    $statePath = Join-Path $stateRoot 'deploy-state.json'
    if (Test-Path -LiteralPath $statePath) {
        $state = Get-Content -Raw -LiteralPath $statePath | ConvertFrom-Json
        if ($state.Version -ne 1 -or [IO.Path]::GetFullPath($state.RedistRoot).TrimEnd('\') -ine $GameRoot.TrimEnd('\')) { throw 'Invalid deployment baseline.' }
        foreach ($entry in $state.Files) {
            if (-not $entry.HadOriginal) { continue }
            if ($entry.Path -notmatch '^(models|textures)/[a-zA-Z0-9_./-]+\.(jpg|tga|png|dds|bmp)$' -or $entry.Path.Contains('..')) { continue }
            $backup = Join-Path (Join-Path $stateRoot 'backup') $entry.Path
            if (-not (Test-Path -LiteralPath $backup) -or (Get-FileHash -LiteralPath $backup).Hash -ine $entry.OriginalHash) { throw "Missing or modified original backup: $($entry.Path)" }
            $index[$entry.Path] = $backup
        }
    }
    return $index
}

function Get-TextureBaselinePath([string]$GameRoot, [hashtable]$Index, [string]$RelativePath) {
    if ($Index.ContainsKey($RelativePath)) { return $Index[$RelativePath] }
    return Join-Path $GameRoot $RelativePath
}

function Get-TextureRGBABytes([int]$Width, [int]$Height) {
    # Exact full mip chain, conservative RGBA storage even for RGB sources.
    $bytes = 0L
    while ($true) {
        $bytes += [long]$Width * $Height * 4
        if ($Width -eq 1 -and $Height -eq 1) { return $bytes }
        $Width = [Math]::Max(1, [int][Math]::Floor($Width / 2))
        $Height = [Math]::Max(1, [int][Math]::Floor($Height / 2))
    }
}
