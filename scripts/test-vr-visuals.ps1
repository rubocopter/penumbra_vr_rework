[CmdletBinding()]
param()
# CPU references and selection guards; not GPU precision or headset validation.
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$script:visualChecks = 0
function Assert-Visual([bool]$condition, [string]$message) {
    $script:visualChecks++
    if (-not $condition) { throw "VR visual regression: $message" }
}
function Get-ToneRGB([double[]]$rgb) {
    $mapped = @(foreach ($c in $rgb) {
        $x = [Math]::Max(0.0,$c) * 1.25
        ($x*(2.51*$x+0.03))/($x*(2.43*$x+0.59)+0.14)
    })
    $luma = $mapped[0]*0.2126 + $mapped[1]*0.7152 + $mapped[2]*0.0722
    return @(foreach ($x in $mapped) {
        $contrasted = (($luma + ($x-$luma)*1.12)-0.5)*1.08+0.5
        [Math]::Pow([Math]::Min(1.0,[Math]::Max(0.0,$contrasted)),0.94)
    })
}
function Get-Sharpened([double[]]$samples) {
    $neighbours = ($samples[1]+$samples[2]+$samples[3]+$samples[4])*0.25
    $localMin = ($samples | Measure-Object -Minimum).Minimum
    $localMax = ($samples | Measure-Object -Maximum).Maximum
    return [Math]::Max(0.0,[Math]::Min($localMax,
        [Math]::Max($localMin,$samples[0]+($samples[0]-$neighbours)*0.65)))
}
$repoRoot = Split-Path -Parent $PSScriptRoot
$programRoot = Join-Path $repoRoot 'HPL1Engine\assets\core\programs'
$final = Get-Content -Raw (Join-Path $programRoot 'VR_Enhanced_Final_fp.cg')
foreach ($required in @('color *= 1.25','2.51 * color + 0.03','2.43 * color + 0.59',
    '+ 0.14','float3(0.2126, 0.7152, 0.0722)', 'color, 1.12',
    '(color - 0.5) * 1.08 + 0.5','pow(max(color, 0.0), 0.94)',
    '(center.xyz - neighbours) * 0.65','localMin, localMax','saturate(color), center.w')) {
    Assert-Visual ($final.Contains($required)) "Final shader/reference drift: $required"
}
$ambient = Get-Content -Raw (Join-Path $programRoot 'Ambient_Hemisphere_fp.cg')
$ambientVP = Get-Content -Raw (Join-Path $programRoot 'Ambient_Hemisphere_vp.cg')
Assert-Visual ($ambient.Contains('oColor.xyz * 0.38 + min(oColor.xyz * 3.0, 0.031)')) 'Bounded ambient preconditioning.'
Assert-Visual ($ambientVP.Contains('lerp(0.65, 0.90') -and $ambientVP.Contains('lerp(0.90, 1.00')) 'Response never exceeds original ambient.'
Assert-Visual ($ambient -notmatch 'oColor\.[wa]\s*=') 'Ambient alpha must not change.'
foreach ($light in (Get-ChildItem -LiteralPath $programRoot -Filter 'VR_*_Light*_fp.cg')) {
    $source=Get-Content -Raw -LiteralPath $light.FullName
    Assert-Visual ($source.Contains('(1.0 - saturate(luma / 0.28)) * 0.06')) "Restrained diffuse recovery: $($light.Name)"
    if ($light.Name -like '*Spec*') {
        Assert-Visual ($source.Contains('24) * 1.00 * lightColor.w') -and -not $source.Contains('24) * 1.35')) "No artificial specular energy boost: $($light.Name)"
    }
}
for ($i=0; $i -le 100; $i++) {
    $sample=$i/100.0
    $weight=1.0-[Math]::Min(1.0,$sample/0.28)
    $recovered=$sample+$weight*0.06
    $old=$sample+$weight*0.18
    Assert-Visual ($recovered -ge $sample -and $recovered -le $old) 'Recovery never exceeds the previous lifted diffuse.'
    if ($sample -ge 0.28) { Assert-Visual ($recovered -eq $sample) 'Non-dark diffuse stays untouched.' }
    $ambientOld=$sample*0.38+[Math]::Min($sample*3,0.028)
    $ambientNew=$sample*0.38+[Math]::Min($sample*3,0.031)
    Assert-Visual ($ambientNew -ge $ambientOld -and $ambientNew-$ambientOld -le 0.003000001) 'Ambient-only lift is capped at +0.003 pre-tone.'
}
$dark0=0.05+(1-0.05/0.28)*0.06
$dark1=0.15+(1-0.15/0.28)*0.06
Assert-Visual (($dark1-$dark0) -gt (0.15-0.05)*0.75) 'Retain at least 75 percent of original dark-diffuse contrast before lighting/tone.'
Assert-Visual (((Get-ToneRGB @(0,0,0)) | Measure-Object -Maximum).Maximum -eq 0) 'Black remains black.'
$previous = 0.0
for ($i=0; $i -le 1024; $i++) {
    $level = [Math]::Pow(10.0,-6.0+$i*(6.0+[Math]::Log10(65504.0))/1024.0)
    $mapped = (Get-ToneRGB @($level,$level,$level))[0]
    Assert-Visual (-not [double]::IsNaN($mapped) -and $mapped -ge 0 -and $mapped -le 1) 'Finite SDR output across RGBA16F range.'
    Assert-Visual ($mapped -ge $previous-1e-12) 'Neutral tone curve is monotonic.'
    $previous=$mapped
}
# Display RGB vs OFF ambient on uniform surfaces, without direct/emissive
# light or sharpening edges. This is not a claim that an arbitrary complete
# ON scene is darker than OFF at every pixel.
$random = New-Object System.Random(1942)
for ($i=0; $i -lt 2048; $i++) {
    $stock = @($random.NextDouble(),$random.NextDouble(),$random.NextDouble())
    if ($i -lt 4) { $stock=@(0.0,0.0,0.0); if ($i -gt 0) { $stock[$i-1]=1.0 } }
    $conditioned = @(foreach ($c in $stock) {
        $response = 0.5733 + $random.NextDouble()*(0.90-0.5733)
        $a=$c*$response
        $a*0.38+[Math]::Min($a*3.0,0.031)
    })
    $display = Get-ToneRGB $conditioned
    for ($channel=0; $channel -lt 3; $channel++) {
        Assert-Visual ($display[$channel] -le $stock[$channel]+1e-12) 'Ambient display must stay below original OFF RGB.'
    }
}
foreach ($level in @(0.0,0.01,0.1,0.5,1.0,8.0)) {
    Assert-Visual ((Get-Sharpened @($level,$level,$level,$level,$level)) -eq $level) 'No sharpening in uniform regions.'
}
Assert-Visual ((Get-Sharpened @(0,1,0,0,0)) -eq 0) 'Do not add a sharpening halo to black.'
for ($i=0; $i -lt 128; $i++) {
    $samples=@(1..5 | ForEach-Object { $random.NextDouble()*8.0 })
    $sharp=Get-Sharpened $samples
    Assert-Visual ($sharp -ge ($samples | Measure-Object -Minimum).Minimum -and
        $sharp -le ($samples | Measure-Object -Maximum).Maximum) 'No new sharpening extrema.'
}
foreach ($name in @('VR_Glowstick_Halo_fp.cg','VR_Glowstick_Halo_Fog_fp.cg')) {
    $halo=Get-Content -Raw (Join-Path $programRoot $name)
    $alpha = if ($name -like '*_Fog_*') { 'tex2D(diffuseMap, uv) * fog * color' } else { 'tex2D(diffuseMap, uv).a * color.a' }
    foreach ($part in @('uv * 2.0 - 1.0','saturate(1.0 - dot(q, q))',
        'envelope * envelope * envelope','half3(0.030, 0.075, 0.055)',$alpha)) {
        Assert-Visual ($halo.Contains($part)) "Halo reference drift: $name / $part"
    }
}
$last=1.0
for ($i=0; $i -le 100; $i++) {
    $r=$i/100.0
    $e=[Math]::Pow([Math]::Max(0,1-$r*$r),3)
    Assert-Visual ($e -ge 0 -and $e -le $last) 'Halo is radially decreasing, with no rings.'
    $last=$e
}
Assert-Visual ($last -eq 0) 'Halo goes to zero at its boundary.'
$hands=Get-Content -Raw (Join-Path $repoRoot 'PenumbraOverture\PlayerHands.cpp')
$state=Get-Content -Raw (Join-Path $repoRoot 'HPL1Engine\sources\graphics\RenderState.cpp')
$renderer=Get-Content -Raw (Join-Path $repoRoot 'HPL1Engine\sources\graphics\Renderer3D.cpp')
Assert-Visual ($hands.Contains('msName == "Glowstick") pLight->SetVREnhancedGain(0.70f)')) 'Glowstick render-only gain.'
Assert-Visual ($hands.Contains('msName == "Flashlight") pLight->SetVREnhancedGain(0.90f)')) 'Flashlight render-only gain.'
Assert-Visual ($state.Contains('if(apSettings->mbVREnhancedVisualsActive)')) 'OFF skips held-light gain.'
Assert-Visual ($renderer.Contains('if(mRenderSettings.mbVREnhancedVisualsActive &&') -and
    $renderer.Contains('static_cast<cBillboard*>(pObject)->GetVREnhancedSoftHalo()')) 'Halo override requires VR activation and explicit HUD tag.'
Write-Host "VR visual CPU reference: $script:visualChecks checks, 0 failures (headset validation still required)." -ForegroundColor Green
