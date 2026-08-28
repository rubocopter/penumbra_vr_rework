[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$requiredPaths = @(
    'PenumbraVR.sln',
    'OALWrapper\OALWrapper.vcxproj',
    'HPL1Engine\HPL.vcxproj',
    'HPL1Engine\include\game\VRTracking.h',
    'PenumbraOverture\Penumbra.vcxproj',
    'PenumbraOverture\VRHaptics.cpp',
    'PenumbraOverture\VRHaptics.h',
    'PenumbraOverture\VRSettings.cpp',
    'PenumbraOverture\VRSettings.h',
    'dependencies\openvr-2.15.6\headers\openvr.h',
    'dependencies\openvr-2.15.6\lib\win32\openvr_api.lib',
    'dependencies\openvr-2.15.6\bin\win32\openvr_api.dll',
    'dependencies\lib\win32\Newton.dll',
    'dependencies\lib\win32\SDL.dll',
    'data\config\English.lang',
    'data\config\Espanol.lang',
    'data\vr\actions.json',
    'data\vr\bindings\psvr2_sense.json',
    'data\vr\bindings\vive_controller.json',
    'data\vr\bindings\knuckles.json',
    'data\vr\bindings\oculus_touch.json',
    'data\vr\bindings\microsoft_motion_controller.json',
    'data\vr\bindings\pico4_controller.json',
    'data\vr\bindings\pico_neo3_controller.json',
    'data\vr\bindings\holographic_controller.json',
    'docs\INPUT_ARCHITECTURE.md',
    'scripts\deploy.ps1'
)

$missingPaths = foreach ($relativePath in $requiredPaths) {
    $candidatePath = Join-Path $repositoryRoot $relativePath
    if (-not (Test-Path -LiteralPath $candidatePath)) {
        $relativePath
    }
}

if ($missingPaths) {
    throw "Missing required repository files:`n - $($missingPaths -join "`n - ")"
}

$projectPaths = @(
    'OALWrapper\OALWrapper.vcxproj',
    'HPL1Engine\HPL.vcxproj',
    'PenumbraOverture\Penumbra.vcxproj'
)

foreach ($relativeProjectPath in $projectPaths) {
    $projectPath = Join-Path $repositoryRoot $relativeProjectPath
    [xml]$projectDocument = Get-Content -Raw -LiteralPath $projectPath
    $projectText = $projectDocument.OuterXml

    if ($projectText -match '(?i)[A-Z]:\\dev\\vrpenumbra') {
        throw "Author-specific absolute path remains in $relativeProjectPath"
    }
    if ($projectText -notmatch '<PlatformToolset>v143</PlatformToolset>') {
        throw "Visual Studio 2022 toolset is not configured in $relativeProjectPath"
    }
    if ($projectText -notmatch '<Platform>Win32</Platform>') {
        throw "The required Win32 target is missing from $relativeProjectPath"
    }
}

function Get-LanguageEntryMap([string]$relativePath) {
    $languagePath = Join-Path $repositoryRoot $relativePath
    $languageDocument = New-Object System.Xml.XmlDocument
    $languageDocument.Load($languagePath)
    $entries = @{}

    foreach ($category in $languageDocument.SelectNodes('/LANGUAGE/CATEGORY')) {
        foreach ($entry in $category.SelectNodes('Entry')) {
            $key = $category.GetAttribute('Name') + '/' + $entry.GetAttribute('Name')
            $entries[$key] = $entry.InnerText
        }
    }

    return $entries
}

$englishEntries = Get-LanguageEntryMap 'data\config\English.lang'
$spanishEntries = Get-LanguageEntryMap 'data\config\Espanol.lang'
$missingSpanishEntries = @($englishEntries.Keys | Where-Object { -not $spanishEntries.ContainsKey($_) } | Sort-Object)
$unexpectedSpanishEntries = @($spanishEntries.Keys | Where-Object { -not $englishEntries.ContainsKey($_) } | Sort-Object)

if ($missingSpanishEntries -or $unexpectedSpanishEntries) {
    $details = @()
    if ($missingSpanishEntries) {
        $details += "Missing Spanish entries:`n - $($missingSpanishEntries -join "`n - ")"
    }
    if ($unexpectedSpanishEntries) {
        $details += "Unexpected Spanish entries:`n - $($unexpectedSpanishEntries -join "`n - ")"
    }
    throw "English and Spanish language keys do not match.`n$($details -join "`n")"
}

# Read every engine and game source exactly once; the checks below each used
# to rescan the whole tree separately.
$vrSourceRoots = @(
    (Join-Path $repositoryRoot 'HPL1Engine\include'),
    (Join-Path $repositoryRoot 'HPL1Engine\sources'),
    (Join-Path $repositoryRoot 'PenumbraOverture')
)
$sourceFiles = Get-ChildItem -LiteralPath $vrSourceRoots -Recurse -Include '*.cpp', '*.h', '*.hpp' -File |
    Sort-Object FullName |
    ForEach-Object {
        [pscustomobject]@{
            FullName = $_.FullName
            RelativePath = $_.FullName.Substring($repositoryRoot.Length + 1)
            Content = Get-Content -Raw -LiteralPath $_.FullName
        }
    }

$literalTranslationKeys = @{}
$translationPattern = 'kTranslate\(\s*"([^"]+)"\s*,\s*"([^"]+)"\s*\)'
foreach ($sourceFile in $sourceFiles) {
    if (-not $sourceFile.RelativePath.StartsWith('PenumbraOverture\')) {
        continue
    }
    foreach ($match in [regex]::Matches($sourceFile.Content, $translationPattern)) {
        $key = $match.Groups[1].Value + '/' + $match.Groups[2].Value
        $literalTranslationKeys[$key] = $true
    }
}

$missingLiteralTranslations = @($literalTranslationKeys.Keys | Where-Object {
    -not $englishEntries.ContainsKey($_) -or -not $spanishEntries.ContainsKey($_)
} | Sort-Object)
if ($missingLiteralTranslations) {
    throw "Literal translations used by the game are missing:`n - $($missingLiteralTranslations -join "`n - ")"
}

$legacyVRSpaceSymbols = @(
    'vr_head_view_mat',
    'vr_head_world_view_mat',
    'vr_player_pos',
    'ViveToWorldSpace'
)

foreach ($legacySymbol in $legacyVRSpaceSymbols) {
    $legacyMatch = $sourceFiles | Where-Object {
        $_.Content.IndexOf($legacySymbol, [System.StringComparison]::OrdinalIgnoreCase) -ge 0
    } | Select-Object -First 1
    if ($legacyMatch) {
        throw "Legacy VR-space symbol '$legacySymbol' bypasses the TrackingToWorld boundary."
    }
}

$hapticOffender = $sourceFiles | Where-Object {
    $_.RelativePath.StartsWith('PenumbraOverture\') -and
    (Split-Path -Leaf $_.RelativePath) -notin @('VRHaptics.cpp', 'VRHaptics.h') -and
    $_.Content.IndexOf('vr_input.TriggerHaptic', [System.StringComparison]::OrdinalIgnoreCase) -ge 0
} | Select-Object -First 1
if ($hapticOffender) {
    throw ("Gameplay haptics must be routed through PenumbraOverture\VRHaptics.cpp " +
        "(found in $($hapticOffender.RelativePath)).")
}

$steamVRInputHeader = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\include\input\SteamVRInput.h')
$steamVRInputSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\sources\input\SteamVRInput.cpp')
$buttonHandlerSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\ButtonHandler.cpp')

foreach ($requiredSemanticInputSnippet in @(
    'struct cVRInputState',
    'UpdateLegacyState',
    'cVRButtonState interact',
    'cVRButtonState crouch',
    'cVRButtonState uiSelect'
)) {
    if (($steamVRInputHeader + $steamVRInputSource) -notmatch [regex]::Escape($requiredSemanticInputSnippet)) {
        throw "The semantic VR input boundary is missing '$requiredSemanticInputSnippet'."
    }
}

if ($steamVRInputSource -match '\.SetButtonState\s*\(') {
    throw 'SteamVR actions must not be transported through TrackedController::ButtonState.'
}

foreach ($legacyButtonField in @(
    'touchContact', 'touchX', 'touchY',
    'padPressed', 'padJustPressed', 'padJustReleased',
    'gripPressed', 'gripJustPressed', 'gripJustReleased',
    'triggerPressed', 'triggerJustPressed', 'triggerJustReleased',
    'menuPressed', 'menuJustPressed', 'menuJustReleased'
)) {
    if ($buttonHandlerSource -match "\b$legacyButtonField\b") {
        throw "ButtonHandler must consume cVRInputState instead of legacy field '$legacyButtonField'."
    }
}

$vrSettingsSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\VRSettings.cpp')
$vrSettingsHeader = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\VRSettings.h')
foreach ($requiredVRSetting in @(
    'MoveSpeed', 'MoveDeadZone', 'HeightOffset', 'PlayerHeight',
    'SnapTurnAngle', 'SmoothTurnSpeed', 'TurnDeadZone',
    'UIDistance', 'UIScale', 'PhysicalCrouchDepth', 'SubtitleScale'
)) {
    if ($vrSettingsSource -notmatch [regex]::Escape("GetFloat(`"VR`", `"$requiredVRSetting`"")) {
        throw "Persistent VR setting '$requiredVRSetting' is not loaded from the [VR] section."
    }
    if ($vrSettingsSource -notmatch [regex]::Escape("SetFloat(`"VR`", `"$requiredVRSetting`"")) {
        throw "Persistent VR setting '$requiredVRSetting' is not saved to the [VR] section."
    }
}
if ($vrSettingsSource -notmatch [regex]::Escape('GetString("VR", "TurnMode"') -or
    $vrSettingsSource -notmatch [regex]::Escape('SetString("VR", "TurnMode"')) {
    throw 'Persistent VR setting TurnMode must be loaded from and saved to the [VR] section.'
}
if ($vrSettingsSource -notmatch [regex]::Escape('GetString("VR", "CrouchMode"') -or
    $vrSettingsSource -notmatch [regex]::Escape('SetString("VR", "CrouchMode"')) {
    throw 'Persistent VR setting CrouchMode must be loaded from and saved to the [VR] section.'
}
foreach ($requiredVRStringSetting in @('Handedness', 'PlayMode')) {
    if ($vrSettingsSource -notmatch [regex]::Escape("GetString(`"VR`", `"$requiredVRStringSetting`"")) {
        throw "Persistent VR setting '$requiredVRStringSetting' is not loaded from the [VR] section."
    }
    if ($vrSettingsSource -notmatch [regex]::Escape("SetString(`"VR`", `"$requiredVRStringSetting`"")) {
        throw "Persistent VR setting '$requiredVRStringSetting' is not saved to the [VR] section."
    }
}

foreach ($requiredVRSetter in @(
    'SetMoveSpeed', 'SetMoveDeadZone', 'SetHeightOffset', 'SetTurnMode',
    'SetSnapTurnAngle', 'SetSmoothTurnSpeed', 'SetTurnDeadZone',
    'SetUIDistance', 'SetUIScale', 'SetCrouchMode',
    'SetPhysicalCrouchDepth', 'SetSubtitleScale',
    'SetHandedness', 'SetPlayMode', 'SetPlayerHeight'
)) {
    if ($vrSettingsHeader -notmatch [regex]::Escape($requiredVRSetter)) {
        throw "The in-game VR settings editor is missing clamped setter '$requiredVRSetter'."
    }
}

$mainMenuSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\MainMenu.cpp')
if ($mainMenuSource -notmatch [regex]::Escape('ApplyVRSettings(true)')) {
    throw 'The in-game VR settings editor must apply and save changes immediately.'
}
if ($mainMenuSource -notmatch [regex]::Escape('eMainMenuState_Options,//eMainMenuState_OptionsVRSettings')) {
    throw 'The VR settings state is missing from the positional menu back-state table.'
}
foreach ($requiredVRMenuTranslation in @(
    'VRHandedness', 'VRPlayMode', 'VRPlayerHeight', 'VRCalibrateHeight',
    'VRLeft', 'VRRight', 'VRStanding', 'VRSeated', 'TipVRCalibrateHeight',
    'VRTurnMode', 'VRSnapAngle', 'VRSmoothSpeed', 'VRTurnDeadZone',
    'VRMoveSpeed', 'VRMoveDeadZone', 'VRHeightOffset', 'VRUIDistance',
    'VRUIScale', 'VRCrouchMode', 'VRPhysicalCrouchDepth', 'VRSubtitleScale',
    'VRDisabled', 'VRSnap', 'VRSmooth', 'VRPhysical', 'VRButton', 'VRHybrid',
    'TipVRAdjust', 'VRControls', 'VRMovement', 'VRCalibration', 'VRDisplay',
    'VRDetectionDepth', 'VRRecenter', 'TipVRRecenter'
)) {
    $translationKey = "MainMenu/$requiredVRMenuTranslation"
    if (-not $englishEntries.ContainsKey($translationKey) -or
        -not $spanishEntries.ContainsKey($translationKey)) {
        throw "The in-game VR settings editor is missing translation '$translationKey'."
    }
}

$initSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Init.cpp')
$gameDeleteIndex = $initSource.IndexOf('hplDelete( mpGame )')
$mirrorSaveIndex = $initSource.IndexOf('mpConfig->SetBool("Game", "RenderToMonitor", mpGame->mbRenderToMonitor)')
if ($gameDeleteIndex -lt 0 -or $mirrorSaveIndex -lt 0 -or $mirrorSaveIndex -gt $gameDeleteIndex) {
    throw 'RenderToMonitor must be read and saved before mpGame is destroyed.'
}

$vrTrackingHeader = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\include\game\VRTracking.h')
foreach ($requiredTrackingOperation in @(
    'SetWorldYaw', 'AddWorldYaw', 'TrackingDirectionToWorld', 'RecenterOrientation',
    'SetPostureOffset', 'mfHeightCalibration + mfPostureOffset'
)) {
    if ($vrTrackingHeader -notmatch [regex]::Escape($requiredTrackingOperation)) {
        throw "TrackingToWorld is missing '$requiredTrackingOperation'."
    }
}

$playerSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Player.cpp')
if ($playerSource -notmatch [regex]::Escape('TrackingDirectionToWorld(headDelta)')) {
    throw 'Physical HMD displacement must be rotated through TrackingToWorld world yaw.'
}
if ($buttonHandlerSource -notmatch [regex]::Escape('GetHeadWorldPose().GetRotation()')) {
    throw 'VR locomotion direction must be derived from the world-space HMD pose.'
}
if ($buttonHandlerSource -notmatch '\b(?:vrInput|aInputState)\.crouch\.justPressed\b') {
    throw 'VR crouch must be consumed through the semantic input state.'
}
if ($playerSource -match [regex]::Escape('// mvStates[mState]->OnStartCrouch()') -or
    $playerSource -match [regex]::Escape('// mvStates[mState]->OnStopCrouch()')) {
    throw 'Penumbra crouch state callbacks must remain active.'
}

$inventorySource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Inventory.cpp')
foreach ($requiredInventoryVRSnippet in @(
    'VRHelper::UpdateUIPointer',
    'VR reticle',
    '(*mpActionVec)[i].c_str()'
)) {
    if ($inventorySource -notmatch [regex]::Escape($requiredInventoryVRSnippet)) {
        throw "The VR inventory interaction is missing '$requiredInventoryVRSnippet'."
    }
}

$vrHelperSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\VRHelper.hpp')
foreach ($requiredPointerSnippet in @(
    'TraceUIPointer',
    'DominantHand(game).IsPoseValid()',
    'OffHand(game).IsPoseValid()',
    'pointerDrawActive = true',
    'pointerDrawStart = rayStart'
)) {
    if ($vrHelperSource -notmatch [regex]::Escape($requiredPointerSnippet)) {
        throw "The shared VR UI pointer is missing '$requiredPointerSnippet'."
    }
}

$penumbraProjectSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Penumbra.vcxproj')
if (($penumbraProjectSource | Select-String -Pattern '<LargeAddressAware>true</LargeAddressAware>' -AllMatches).Matches.Count -ne 2) {
    throw 'Penumbra_vr.exe must enable Large Address Aware in both Win32 configurations.'
}
if (($penumbraProjectSource | Select-String -Pattern ([regex]::Escape('<AdditionalOptions>/FS %(AdditionalOptions)</AdditionalOptions>')) -AllMatches).Matches.Count -ne 2) {
    throw 'Full parallel Penumbra rebuilds must serialize compiler PDB writes with /FS in both Win32 configurations.'
}

$buildScriptSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'scripts\build.ps1')
foreach ($requiredRebuildGuardSnippet in @(
    'Get-BuildInputFingerprint',
    '.rebuild-guard-',
    "'/t:Build'",
    "'/t:Rebuild'"
)) {
    if ($buildScriptSource -notmatch [regex]::Escape($requiredRebuildGuardSnippet)) {
        throw ("The build must force a full rebuild whenever header or project inputs changed, " +
            "because class-layout changes cannot reuse ABI-stale object files (missing guard snippet: '$requiredRebuildGuardSnippet').")
    }
}

$sceneSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\sources\scene\Scene.cpp')
if ($sceneSource -notmatch [regex]::Escape('mVRMainUIAnchor = head_mat') -or
    $sceneSource -notmatch [regex]::Escape('MatrixMul(mVRMainUIAnchor, uiViewMat)')) {
    throw 'Fullscreen VR surfaces must remain anchored in room space.'
}

$mapHandlerSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\MapHandler.cpp')
$mapLoadTextSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\MapLoadText.cpp')
$saveHandlerSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\SaveHandler.cpp')
foreach ($requiredLoadingSnippet in @(
    'BeginMapLoad()', 'BeginInitialMapLoad(', 'CompletePendingMapChange()',
    'BeginVRBlockingLoad()', 'CompletePendingGameLoad()',
    'FadeToColor(0.08f', 'mbLoadingFramePresented = true'
)) {
    if (($mapHandlerSource + $mapLoadTextSource + $saveHandlerSource) -notmatch [regex]::Escape($requiredLoadingSnippet)) {
        throw "The staged VR loading path is missing '$requiredLoadingSnippet'."
    }
}

$introStorySource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\IntroStory.cpp')
if ($introStorySource -match [regex]::Escape('DrawLoadingScreen(""')) {
    throw 'The intro must enter the staged VR loading path instead of the desktop loading swap.'
}
foreach ($requiredInitialLoadRoute in @(
    'BeginInitialMapLoad(mpInit->msStartMap,mpInit->msStartLink)',
    'BeginInitialMapLoad("level00_00_vr_tutorial.dae", "link01")'
)) {
    if (($introStorySource + $mainMenuSource) -notmatch [regex]::Escape($requiredInitialLoadRoute)) {
        throw "An initial VR load still bypasses the staged path: '$requiredInitialLoadRoute'."
    }
}

$playerHeader = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Player.h')
$vrInteractionSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\PlayerState_Misc_VR.cpp')
$vrHeldObjectSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\PlayerState_Interact_VR.cpp')
foreach ($requiredHandStateSnippet in @(
    'cVRHandTarget mVRHandTargets[eVRHandIndex_Count]',
    'iPhysicsBody *mVRHeldBodies[eVRHandIndex_Count]',
    'GetVRHandTarget(',
    'vr_dominant_hand == eSteamVRHand_Left',
    'GetVRHandInteractionShape()',
    'SetVRHeldBody(interactHand'
)) {
    if (($playerHeader + $vrInteractionSource + $vrHeldObjectSource) -notmatch [regex]::Escape($requiredHandStateSnippet)) {
        throw "Independent VR hand state is missing '$requiredHandStateSnippet'."
    }
}
if ($vrInteractionSource -notmatch [regex]::Escape('if (!mpInit->mpPlayerHands->GetHandPalmPose(i, poseMatrix))')) {
    throw 'VR target detection must skip a hand whose tracked pose is invalid.'
}
if ($playerSource -notmatch [regex]::Escape('mpVRHandInteractionShape = mpScene->GetWorld3D()->GetPhysicsWorld()->CreateBoxShape')) {
    throw 'The VR hand interaction shape must be owned for the lifetime of the player physics world.'
}
if ($playerSource -match '(?s)void cPlayer::Reset\(\).*?DestroyWorldObjects\(\);' -or
    $playerSource -notmatch '(?s)void cPlayer::Reset\(\).*?mpCharBody = NULL;.*?mpVRHandInteractionShape = NULL;' -or
    $playerSource -notmatch '(?s)void cPlayer::OnWorldExit\(\).*?DestroyWorldObjects\(\);') {
    throw 'Player reset must leave complete-world teardown to MapHandler; only normal world exit destroys individual player physics objects.'
}
if ($playerSource -match [regex]::Escape('mpScene->GetWorld3D()->GetPhysicsWorld()->DestroyShape(mpVRHandInteractionShape)')) {
    throw 'VR hand geometry destruction must tolerate an already-null scene world.'
}

$vrHandCollisionPolicy = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\VRHandCollisionPolicy.h')
$vrMathTests = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'tests\VRTrackingTest\main.cpp')
foreach ($requiredHandCollisionSnippet in @(
    'kCollisionSizeX = 0.190f',
    'kCollisionSizeY = 0.052f',
    'kCollisionSizeZ = 0.125f',
    'kInteractionContactTolerance = 0.008f',
	'kMaxTrackedHandDistanceFromHead = 2.50f',
    'CheckVRHandWorldCollision',
	'IsUsableVRHandPose',
	'InterpolateVRHandRotation',
	'aRawPose == mVRHandRawPoses[lHand]',
	'if(bFirstPose)',
	'bInitialOverlap',
	'FindVRHandRecoveryPose',
	'ShouldUseRecoveryAnchor',
    'TestVRHandCannotCrossWall',
    'TestVRHandCannotCrossClosedDoorAtSpeed',
	'TestVRHandInitialCeilingOverlapRecoversBySweep',
	'TestVRHandRecoveryCannotCrossClosedDoor',
	'TestVRHandHingeRecoveryRequiresPullback'
)) {
    if (($vrHandCollisionPolicy + $playerSource + $vrMathTests) -notmatch [regex]::Escape($requiredHandCollisionSnippet)) {
        throw "The VR hand collision regression guard is missing '$requiredHandCollisionSnippet'."
    }
}
if ($playerSource -match 'fMaxVisualLag|vRawPos\s*\+\s*vVisualLag') {
    throw 'VR hand collision must never follow the raw controller through a barrier to cap visual lag.'
}
if ($playerSource -notmatch [regex]::Escape('pSkipBody = mVRHeldBodies[lHand]') -or
    $playerSource -match 'pSkipBody\s*=\s*handTarget\.mpBody') {
    throw 'Interaction assistance may soften contact skin but must not skip its target or a closed door.'
}
$handPoseCacheIndex = $playerSource.IndexOf('aRawPose == mVRHandRawPoses[lHand]')
$handOverlapIndex = $playerSource.IndexOf('kOverlapResolveIterations', $handPoseCacheIndex)
if ($handPoseCacheIndex -lt 0 -or $handOverlapIndex -le $handPoseCacheIndex) {
    throw 'Repeated consumers of one tracking sample must reuse the pose before any physics query.'
}
$firstPoseIndex = $playerSource.IndexOf('if(bFirstPose)', $handPoseCacheIndex)
$firstPoseRawIndex = $playerSource.IndexOf('vStartPos = vRawPos', $firstPoseIndex)
if ($firstPoseIndex -lt 0 -or $firstPoseRawIndex -le $firstPoseIndex) {
    throw 'A collision-free first tracking sample must initialize at the controller.'
}
if ($playerSource -notmatch '(?s)bInitialOverlap\s*=\s*CheckVRHandWorldCollision.*?FindVRHandRecoveryPose' -or
    $playerSource -notmatch [regex]::Escape('bFirstPose && bInitialOverlap')) {
    throw 'A first hand pose embedded by loading must recover from a collision-free body-side anchor.'
}

$oalEffectSlotSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\sources\OAL_EffectSlot.cpp')
$oalEffectSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\sources\OAL_Effect.cpp')
$oalSourceSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\sources\OAL_Source.cpp')
$oalSourceManagerSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\sources\OAL_SourceManager.cpp')
$oalDeviceSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'OALWrapper\sources\OAL_Device.cpp')
$lowLevelSoundSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\sources\impl\LowLevelSoundOpenAL.cpp')
$gameInitSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Init.cpp')
foreach ($requiredAudioShutdownSnippet in @(
    'mpMutex(SDL_CreateMutex())',
    'SDL_DestroyMutex(mpMutex)',
    'mpMutex = NULL',
    'if(mpStreamListMutex)',
    'mpStreamListMutex = NULL'
)) {
    if (($oalEffectSlotSource + $oalEffectSource + $oalSourceManagerSource) -notmatch [regex]::Escape($requiredAudioShutdownSnippet)) {
        throw "The OpenAL/SDL shutdown regression guard is missing '$requiredAudioShutdownSnippet'."
    }
}
$streamMutexCreateIndex = $oalSourceManagerSource.IndexOf('mpStreamListMutex = SDL_CreateMutex')
$streamThreadCreateIndex = $oalSourceManagerSource.IndexOf('mpUpdaterThread = SDL_CreateThread')
if ($streamMutexCreateIndex -lt 0 -or $streamThreadCreateIndex -le $streamMutexCreateIndex) {
    throw 'The legacy OpenAL updater must create its mutex before starting the SDL thread.'
}
if ($oalEffectSlotSource -match 'if\s*\(mpEFXManager->IsThreadAlive\(\)\)\s*mpMutex\s*=\s*SDL_CreateMutex') {
    throw 'Effect-slot mutex lifetime must not depend on the updater thread state.'
}
if ($oalSourceSource -match 'if\s*\(\s*mpSourceManager->IsThreadAlive\(\)\s*\)\s*SDL_(Lock|Unlock)Mutex') {
    throw 'A source must unlock an acquired SDL mutex even after its updater stop flag changes.'
}
if ($oalSourceManagerSource -match 'if\s*\(\s*mbUseThreading\s*\)\s*SDL_(Lock|Unlock)Mutex') {
    throw 'The stream list must unlock its SDL mutex independently of the updater stop flag.'
}
if ($gameInitSource -notmatch 'mbUseSoundThreading\s*=\s*false') {
    throw 'The unsupported legacy OpenAL updater thread must remain disabled in the game configuration.'
}
if ($oalSourceSource -notmatch [regex]::Escape('GetEFXManager()->DestroyFilter(mpFilter)') -or
    $oalSourceSource -match '(?m)^\s*delete\s+mpFilter\s*;') {
    throw 'Source-owned EFX filters must be removed through EFXManager to prevent a shutdown double free.'
}
if ($oalDeviceSource -notmatch '(?s)mpEFXManager\(NULL\).*?mbEFXActive\(false\)' -or
    $oalDeviceSource -notmatch '(?s)if\s*\(!mbEFXActive\).*?delete mpEFXManager;.*?mpEFXManager = NULL;') {
    throw 'Failed EFX initialization must leave an explicitly inactive, null manager.'
}
if ($lowLevelSoundSource -notmatch 'mbNullEffectAttached\s*=\s*false;\s*mpEffect\s*=\s*NULL;') {
    throw 'The low-level environmental effect handle must start null.'
}

$moveStateStart = $vrHeldObjectSource.IndexOf('void cPlayerState_Move_VR::EnterState')
$moveStateEnd = $vrHeldObjectSource.IndexOf('void cPlayerState_Move_VR::OnPostSceneDraw')
if ($moveStateStart -lt 0 -or $moveStateEnd -le $moveStateStart) {
    throw 'Could not inspect the VR move interaction state.'
}
$moveStateSource = $vrHeldObjectSource.Substring($moveStateStart, $moveStateEnd - $moveStateStart)
if ($moveStateSource -match 'SetCollidePlayer\s*\(\s*false\s*\)') {
    throw 'The VR move state must not disable player collision on held swing doors.'
}
if ($moveStateSource -notmatch [regex]::Escape('eGameEntityType_SwingDoor') -or
    $moveStateSource -notmatch [regex]::Escape('SetCollidePlayer(true)')) {
    throw 'The VR move state must explicitly preserve collision for swing doors.'
}

if ($introStorySource -match 'mpBlackTexture|effect_black\.bmp' -or
    $introStorySource -notmatch [regex]::Escape('for(int i=0; i<6; ++i) mpLowGfx->SetTexture(i,NULL);') -or
    $introStorySource -notmatch [regex]::Escape('mpLowGfx->SetBlendActive(false)')) {
    throw 'Cinematics must not draw opaque subtitle bands and must restore render state independently of subtitle visibility.'
}

$sceneHeader = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\include\scene\Scene.h')
$sceneSource = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'HPL1Engine\sources\scene\Scene.cpp')
foreach ($requiredUISnippet in @(
    'SetVRMainUIDistance', 'SetVRMainUIScale',
    '-mfVRMainUIDistance', 'mfVRMainUIScale / 650.0f'
)) {
    if (($sceneHeader + $sceneSource) -notmatch [regex]::Escape($requiredUISnippet)) {
        throw "Configurable VR UI placement is missing '$requiredUISnippet'."
    }
}

$vrDataRoot = Join-Path $repositoryRoot 'data\vr'
$actionManifestPath = Join-Path $vrDataRoot 'actions.json'
$actionManifest = Get-Content -Raw -LiteralPath $actionManifestPath | ConvertFrom-Json
$actionNames = @($actionManifest.actions | ForEach-Object { $_.name })
$actionSetNames = @($actionManifest.action_sets | ForEach-Object { $_.name })

if (($actionNames | Sort-Object -Unique).Count -ne $actionNames.Count) {
    throw 'Duplicate SteamVR action names were found in data\vr\actions.json.'
}
if (($actionSetNames | Sort-Object -Unique).Count -ne $actionSetNames.Count) {
    throw 'Duplicate SteamVR action set names were found in data\vr\actions.json.'
}

foreach ($skeletonAction in @($actionManifest.actions | Where-Object { $_.type -eq 'skeleton' })) {
    $expectedSkeleton = if ($skeletonAction.name -match '/in/left_') { '/skeleton/hand/left' } else { '/skeleton/hand/right' }
    if ($skeletonAction.skeleton -ne $expectedSkeleton) {
        throw "SteamVR action '$($skeletonAction.name)' must declare 'skeleton': '$expectedSkeleton' or SteamVR rejects the whole manifest."
    }
}

foreach ($localization in $actionManifest.localization) {
    $localizedNames = @($localization.PSObject.Properties.Name)
    foreach ($requiredName in @($actionSetNames + $actionNames)) {
        if ($localizedNames -notcontains $requiredName) {
            throw "SteamVR localization '$($localization.language_tag)' is missing '$requiredName'."
        }
    }
}

foreach ($defaultBinding in $actionManifest.default_bindings) {
    $bindingRelativePath = $defaultBinding.binding_url.Replace('/', [System.IO.Path]::DirectorySeparatorChar)
    $bindingPath = Join-Path $vrDataRoot $bindingRelativePath
    if (-not (Test-Path -LiteralPath $bindingPath)) {
        throw "SteamVR default binding does not exist: $bindingRelativePath"
    }

    $binding = Get-Content -Raw -LiteralPath $bindingPath | ConvertFrom-Json
    if ($binding.controller_type -ne $defaultBinding.controller_type) {
        throw "SteamVR binding controller type mismatch in $bindingRelativePath."
    }

    foreach ($bindingSetProperty in $binding.bindings.PSObject.Properties) {
        if ($actionSetNames -notcontains $bindingSetProperty.Name) {
            throw "Unknown SteamVR action set '$($bindingSetProperty.Name)' in $bindingRelativePath."
        }

        $bindingSet = $bindingSetProperty.Value
        foreach ($directBindingCollection in @('haptics', 'poses', 'skeleton')) {
            foreach ($directBinding in @($bindingSet.$directBindingCollection)) {
                if ($null -eq $directBinding) {
                    continue
                }
                if ($actionNames -notcontains $directBinding.output) {
                    throw "Unknown SteamVR action '$($directBinding.output)' in $bindingRelativePath."
                }
                if ($directBindingCollection -eq 'skeleton') {
                    $handSuffix = if ($directBinding.output -match '/in/(left|right)_skeleton$') { $Matches[1] } else { '' }
                    $expectedPath = if ($handSuffix) { "/user/hand/$handSuffix/input/skeleton/$handSuffix" } else { $null }
                    if ($directBinding.path -ne $expectedPath) {
                        throw "Invalid SteamVR skeleton device path '$($directBinding.path)' in $bindingRelativePath; expected '$expectedPath'."
                    }
                }
            }
        }

        foreach ($source in @($bindingSet.sources)) {
            if ($null -eq $source) {
                continue
            }
            foreach ($inputProperty in $source.inputs.PSObject.Properties) {
                $output = $inputProperty.Value.output
                if ($actionNames -notcontains $output) {
                    throw "Unknown SteamVR action '$output' in $bindingRelativePath."
                }
            }
        }
    }

    $boundOutputs = @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.haptics }) |
            ForEach-Object { if ($null -ne $_) { $_.output } }
    ) + @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.poses }) |
            ForEach-Object { if ($null -ne $_) { $_.output } }
    ) + @(
        @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.sources }) |
            ForEach-Object { if ($null -ne $_ -and $null -ne $_.inputs) { $_.inputs.PSObject.Properties.Value.output } }
    )

    $mandatoryActions = @($actionManifest.actions | Where-Object {
        $_.PSObject.Properties.Name -contains 'requirement' -and $_.requirement -eq 'mandatory'
    } | ForEach-Object { $_.name })
    foreach ($mandatoryAction in $mandatoryActions) {
        if ($boundOutputs -notcontains $mandatoryAction) {
            throw "Mandatory SteamVR action '$mandatoryAction' has no input binding in $bindingRelativePath."
        }
    }

    $skeletalControllerTypes = @('playstation_vr2_sense', 'knuckles', 'oculus_touch', 'pico4_controller', 'pico_neo3_controller')
    if ($defaultBinding.controller_type -in $skeletalControllerTypes) {
        $skeletonOutputs = @($binding.bindings.PSObject.Properties | ForEach-Object { $_.Value.skeleton } |
            ForEach-Object { if ($null -ne $_) { $_.output } })
        foreach ($skeletonAction in @('/actions/global/in/left_skeleton', '/actions/global/in/right_skeleton')) {
            if ($skeletonOutputs -notcontains $skeletonAction) {
                throw "Skeletal controller type '$($defaultBinding.controller_type)' must bind '$skeletonAction' in $bindingRelativePath."
            }
        }
    }
}

# Release metadata is deliberately stored in the game log identifier, README,
# and release history. Keep those public surfaces synchronized so a packaged
# executable can always be matched to the documentation users downloaded.
$initText = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'PenumbraOverture\Init.h')
$readmeText = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'readme.md')
$releaseHistoryText = Get-Content -Raw -LiteralPath (Join-Path $repositoryRoot 'docs\RELEASES.md')
if ($initText -notmatch '#define\s+PENUMBRA_VR_VERSION\s+"([^"]+)"') {
    throw 'PENUMBRA_VR_VERSION is missing from PenumbraOverture\Init.h.'
}
$releaseVersion = $Matches[1]
if ($readmeText -notmatch [regex]::Escape("current release: **$releaseVersion**")) {
    throw "README current release does not match $releaseVersion."
}
if ($releaseHistoryText -notmatch [regex]::Escape("## $releaseVersion ")) {
    throw "Release history has no section for $releaseVersion."
}

Write-Host 'Project validation passed.' -ForegroundColor Green
