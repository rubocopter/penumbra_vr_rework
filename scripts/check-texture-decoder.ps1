[CmdletBinding()]
param([string]$ContentRoot)
# Exercise the shipped legacy SDL_image decoder, not just Windows GDI+.
# No GL context, game launch, headset or installation changes.
Set-StrictMode -Version Latest
$ErrorActionPreference='Stop'
if ([IntPtr]::Size -ne 4) {
    $x86PowerShell=Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    $arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-File',$PSCommandPath)
    if ($ContentRoot) { $arguments+=@('-ContentRoot',$ContentRoot) }
    & $x86PowerShell @arguments
    if ($LASTEXITCODE -ne 0) { throw 'Legacy texture decoder check failed.' }
    return
}
$repoRoot=Split-Path -Parent $PSScriptRoot
if (-not $ContentRoot) { $ContentRoot=Join-Path $repoRoot 'data' }
$manifest=Get-Content -Raw (Join-Path $repoRoot 'docs\TEXTURE_SELECTION.json') | ConvertFrom-Json
if (-not ('TextureDecoderCheck.Native' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace TextureDecoderCheck {
    public static class Native {
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern bool SetDllDirectoryW(string path);
        [DllImport("kernel32.dll", CharSet=CharSet.Unicode)] public static extern IntPtr LoadLibraryW(string path);
        [DllImport("SDL.dll", CallingConvention=CallingConvention.Cdecl)] public static extern int SDL_Init(uint flags);
        [DllImport("SDL.dll", CallingConvention=CallingConvention.Cdecl)] public static extern void SDL_Quit();
        [DllImport("SDL.dll", CallingConvention=CallingConvention.Cdecl)] public static extern void SDL_FreeSurface(IntPtr surface);
        [DllImport("SDL.dll", CallingConvention=CallingConvention.Cdecl)] public static extern IntPtr SDL_GetError();
        [DllImport("SDL_image.dll", CallingConvention=CallingConvention.Cdecl, CharSet=CharSet.Ansi)] public static extern IntPtr IMG_Load(string path);
    }
}
'@
}
$dllRoot=Join-Path $repoRoot 'dependencies\lib\win32'
if (-not [TextureDecoderCheck.Native]::SetDllDirectoryW($dllRoot)) { throw 'Cannot set decoder DLL search path.' }
foreach($dll in @('SDL.dll','SDL_image.dll')) {
    if ([TextureDecoderCheck.Native]::LoadLibraryW((Join-Path $dllRoot $dll)) -eq [IntPtr]::Zero) { throw "Cannot load $dll" }
}
if ([TextureDecoderCheck.Native]::SDL_Init(0) -ne 0) { throw 'SDL initialization failed.' }
try {
    foreach($file in $manifest.Files) {
        $surface=[TextureDecoderCheck.Native]::IMG_Load((Join-Path $ContentRoot $file.Path))
        if ($surface -eq [IntPtr]::Zero) {
            $errorText=[Runtime.InteropServices.Marshal]::PtrToStringAnsi([TextureDecoderCheck.Native]::SDL_GetError())
            throw "SDL_image cannot decode $($file.Path): $errorText"
        }
        try {
            # SDL 1.2 Win32: flags, format*, w, h. Format starts with palette*.
            $width=[Runtime.InteropServices.Marshal]::ReadInt32($surface,8)
            $height=[Runtime.InteropServices.Marshal]::ReadInt32($surface,12)
            $format=[Runtime.InteropServices.Marshal]::ReadIntPtr($surface,4)
            $bpp=[Runtime.InteropServices.Marshal]::ReadByte($format,4)
            if ($width -ne $file.Width -or $height -ne $file.Height -or $bpp -ne 24) { throw "Legacy decoder format mismatch: $($file.Path)" }
        } finally { [TextureDecoderCheck.Native]::SDL_FreeSurface($surface) }
    }
} finally { [TextureDecoderCheck.Native]::SDL_Quit(); [void][TextureDecoderCheck.Native]::SetDllDirectoryW($null) }
Write-Host "Legacy SDL_image decoded all $($manifest.Files.Count) selected JPEGs as expected (RGB24, dimensions verified)."
