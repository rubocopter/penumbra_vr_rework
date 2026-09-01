[CmdletBinding()]
param()

# Offline compilation against the same legacy Cg runtime shipped by HPL1.
# No OpenGL context, headset, game startup or writes to shader assets required.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
if ([IntPtr]::Size -ne 4) {
    $x86PowerShell = Join-Path $env:WINDIR 'SysWOW64\WindowsPowerShell\v1.0\powershell.exe'
    & $x86PowerShell -NoProfile -ExecutionPolicy Bypass -File $PSCommandPath
    if ($LASTEXITCODE -ne 0) { throw 'Offline VR shader compilation failed.' }
    return
}

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$cgPath = Join-Path $repositoryRoot 'dependencies\lib\win32\cg.dll'
if (-not (Test-Path -LiteralPath $cgPath)) { throw "Cg runtime missing: $cgPath" }

if (-not ('VRShaderCheck.Cg' -as [type])) {
    Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
namespace VRShaderCheck {
    public static class Cg {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        public static extern IntPtr LoadLibraryW(string path);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr cgCreateContext();
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void cgDestroyContext(IntPtr context);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern int cgGetProfile(string name);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl, CharSet = CharSet.Ansi)]
        public static extern IntPtr cgCreateProgramFromFile(IntPtr context, int source,
            string file, int profile, string entry, IntPtr options);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr cgGetLastListing(IntPtr context);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern int cgGetError();
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr cgGetErrorString(int error);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern IntPtr cgGetProgramString(IntPtr program, int kind);
        [DllImport("cg.dll", CallingConvention = CallingConvention.Cdecl)]
        public static extern void cgDestroyProgram(IntPtr program);
    }
}
'@
}
if ([VRShaderCheck.Cg]::LoadLibraryW($cgPath) -eq [IntPtr]::Zero) {
    throw "Cannot load bundled x86 Cg runtime: $cgPath"
}
$programRoot = Join-Path $repositoryRoot 'HPL1Engine\assets\core\programs'
$shaders = @(Get-ChildItem -LiteralPath $programRoot -File | Where-Object {
    $_.Name -like 'VR_*_fp.cg' -or $_.Name -like 'VR_*_vp.cg' -or
    $_.Name -like 'Ambient_Hemisphere_*.cg'
} | Sort-Object Name)
if ($shaders.Count -ne 16) { throw "Expected 16 VR shaders, found $($shaders.Count)." }
$expectedFetches = @{
    'Ambient_Hemisphere_fp.cg' = 1; 'Ambient_Hemisphere_vp.cg' = 0
    'VR_Bump_Light_fp.cg' = 3; 'VR_Bump_Light_Spot_fp.cg' = 5
    'VR_BumpColorSpec_Light_fp.cg' = 4; 'VR_BumpColorSpec_Light_Spot_fp.cg' = 6
    'VR_BumpSpec_Light_fp.cg' = 3; 'VR_BumpSpec_Light_Spot_fp.cg' = 5
    'VR_Diffuse_Light_fp.cg' = 2; 'VR_Diffuse_Light_Spot_fp.cg' = 4
    'VR_DiffuseSpec_Light_fp.cg' = 2; 'VR_DiffuseSpec_Light_Spot_fp.cg' = 4
    'VR_Enhanced_Final_fp.cg' = 5; 'VR_Enhanced_Final_vp.cg' = 0
    'VR_Glowstick_Halo_fp.cg' = 1; 'VR_Glowstick_Halo_Fog_fp.cg' = 2
}
$context = [VRShaderCheck.Cg]::cgCreateContext()
if ($context -eq [IntPtr]::Zero) { throw 'Cg context creation failed.' }
try {
    foreach ($shader in $shaders) {
        $profileName = if ($shader.Name -like '*_vp.cg') { 'vp40' } else { 'fp40' }
        $cgProfile = [VRShaderCheck.Cg]::cgGetProfile($profileName)
        # CG_SOURCE and CG_COMPILED_PROGRAM values come from cg_enums.h.
        $program = [VRShaderCheck.Cg]::cgCreateProgramFromFile(
            $context, 4112, $shader.FullName, $cgProfile, 'main', [IntPtr]::Zero)
        try {
            $cgError = [VRShaderCheck.Cg]::cgGetError()
            if ($program -eq [IntPtr]::Zero -or $cgError -ne 0) {
                $errorText = [Runtime.InteropServices.Marshal]::PtrToStringAnsi([VRShaderCheck.Cg]::cgGetErrorString($cgError))
                $listing = [Runtime.InteropServices.Marshal]::PtrToStringAnsi([VRShaderCheck.Cg]::cgGetLastListing($context))
                throw "$($shader.Name): $errorText`n$listing"
            }
            $assembly = [Runtime.InteropServices.Marshal]::PtrToStringAnsi([VRShaderCheck.Cg]::cgGetProgramString($program, 4106))
            if ([string]::IsNullOrEmpty($assembly)) { throw "No compiled program: $($shader.Name)" }
            $fetchCount = [regex]::Matches($assembly, '(?m)^\s*(TEX|TXP|TXL|TXB)\s').Count
            if ($fetchCount -ne $expectedFetches[$shader.Name]) {
                throw "Unexpected texture fetch count in $($shader.Name): $fetchCount."
            }
            $expLogCount = [regex]::Matches($assembly, '(?m)^\s*(EX2|LG2|POW)[A-Z]*(?:\.[A-Z]+)?\s').Count
            if ($shader.Name -eq 'VR_Enhanced_Final_fp.cg' -and $expLogCount -ne 3) {
                throw 'The restored dark final pass must compile to three scalar gamma powers.'
            }
            Write-Host ("Cg OK: {0} [{1}, {2} texture instructions, {3} exp/log instructions]" -f $shader.Name, $profileName, $fetchCount, $expLogCount)
        } finally {
            if ($program -ne [IntPtr]::Zero) { [VRShaderCheck.Cg]::cgDestroyProgram($program) }
        }
    }
} finally {
    [VRShaderCheck.Cg]::cgDestroyContext($context)
}
Write-Host 'All 16 VR shaders compiled offline. Driver loading and headset appearance still require runtime testing.' -ForegroundColor Green
