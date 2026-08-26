<#
.SYNOPSIS
    Generates the SteamVR controller binding files in data/vr/bindings from a
    single declarative source in this script.

.DESCRIPTION
    Hand-editing eight near-identical binding files made every new control a
    sixteen-place edit. This script is now the only place bindings are edited:
    each supported controller family declares its physical input names once,
    plus an explicit ordered row list per action set, and the files are
    emitted from that.

    Regenerating must always produce content semantically identical to what a
    reviewer approved; -Check compares freshly generated output against the
    files on disk and fails on any drift, which keeps manual hot-edits from
    silently diverging from the spec.

    Run './scripts/generate-bindings.ps1' to rewrite the files, or
    './scripts/generate-bindings.ps1 -Check' (used by the build) to verify.
#>
[CmdletBinding()]
param(
    # Compare generated output with the files on disk instead of rewriting them.
    [switch]$Check,

    # Write generated files here (defaults to the repository's data/vr/bindings).
    [string]$OutputRoot
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputRoot) {
    $OutputRoot = Join-Path $repositoryRoot 'data\vr\bindings'
}

# ---------------------------------------------------------------------------
# Physical input vocabulary per controller family.
# ---------------------------------------------------------------------------

function New-Controller {
    param(
        [string]$FileName,
        [string]$ControllerType,
        [string]$Name,
        [string]$Description,
        [string]$GripPose,            # device path suffix used for /pose/grip actions
        [bool]$Skeletal,              # bind the skeleton actions
        [string]$FamilyKey,           # family key into $families
        [hashtable]$InputMap             # logical input name -> @{ path = suffix; mode = SteamVR mode }
    )

    [pscustomobject]@{
        FileName       = $FileName
        ControllerType = $ControllerType
        Name           = $Name
        Description    = $Description
        GripPose       = $GripPose
        Skeletal       = $Skeletal
        Family         = $FamilyKey
        Input          = $InputMap
    }
}

$inputGeneric = @{
    stickLeft      = @{ path = 'input/joystick'; mode = 'joystick' }
    stickRight     = @{ path = 'input/joystick'; mode = 'joystick' }
    triggerLeft    = @{ path = 'input/trigger'; mode = 'trigger' }
    triggerRight   = @{ path = 'input/trigger'; mode = 'trigger' }
    gripLeft       = @{ path = 'input/grip'; mode = 'button' }
    gripRight      = @{ path = 'input/grip'; mode = 'button' }
    primaryLeft    = @{ path = 'input/a'; mode = 'button' }
    primaryRight   = @{ path = 'input/a'; mode = 'button' }
    secondaryLeft  = @{ path = 'input/b'; mode = 'button' }
    secondaryRight = @{ path = 'input/b'; mode = 'button' }
    systemLeft     = @{ path = 'input/system'; mode = 'button' }
    trackpadLeft   = @{ path = 'input/trackpad'; mode = 'trackpad' }
    trackpadRight  = @{ path = 'input/trackpad'; mode = 'trackpad' }
}

# Touch-family controllers split the face buttons per hand. The shipped
# recenter binding also uses X on the right hand, which physically has A/B;
# SteamVR accepts the path, so it is reproduced verbatim.
$inputOculus = $inputGeneric.Clone()
$inputOculus.primaryLeft   = @{ path = 'input/x'; mode = 'button' }
$inputOculus.secondaryLeft = @{ path = 'input/y'; mode = 'button' }
$inputOculus.recenterRight = @{ path = 'input/x'; mode = 'button' }

# Vive wands: the trackpad is the only analog surface, so it hosts movement;
# the system button is reserved by SteamVR, leaving pause unbound there.
$inputVive = @{
    stickLeft    = @{ path = 'input/trackpad'; mode = 'trackpad' }
    stickRight   = @{ path = 'input/trackpad'; mode = 'trackpad' }
    triggerLeft  = @{ path = 'input/trigger'; mode = 'trigger' }
    triggerRight = @{ path = 'input/trigger'; mode = 'trigger' }
    gripLeft     = @{ path = 'input/grip'; mode = 'button' }
    gripRight    = @{ path = 'input/grip'; mode = 'button' }
    menuLeft     = @{ path = 'input/application_menu'; mode = 'button' }
    menuRight    = @{ path = 'input/application_menu'; mode = 'button' }
}

# WMR-style controllers: application_menu replaces the face cluster, the right
# trackpad hosts pause, and jump lives on the off-hand trigger.
$inputWmr = @{
    stickLeft     = @{ path = 'input/joystick'; mode = 'joystick' }
    stickRight    = @{ path = 'input/joystick'; mode = 'joystick' }
    triggerLeft   = @{ path = 'input/trigger'; mode = 'trigger' }
    triggerRight  = @{ path = 'input/trigger'; mode = 'trigger' }
    gripLeft      = @{ path = 'input/grip'; mode = 'button' }
    gripRight     = @{ path = 'input/grip'; mode = 'button' }
    menuLeft      = @{ path = 'input/application_menu'; mode = 'button' }
    menuRight     = @{ path = 'input/application_menu'; mode = 'button' }
    trackpadLeft  = @{ path = 'input/trackpad'; mode = 'trackpad' }
    trackpadRight = @{ path = 'input/trackpad'; mode = 'trackpad' }
}

# PS VR2 Sense: PlayStation naming throughout, analog L1/R1 grips bound as
# triggers, and the Create button reaching recenter through the global set.
$inputPsvr2 = @{
    stickLeft      = @{ path = 'input/left_stick'; mode = 'joystick' }
    stickRight     = @{ path = 'input/right_stick'; mode = 'joystick' }
    triggerLeft    = @{ path = 'input/l2'; mode = 'trigger' }
    triggerRight   = @{ path = 'input/r2'; mode = 'trigger' }
    gripLeft       = @{ path = 'input/l1'; mode = 'trigger' }
    gripRight      = @{ path = 'input/r1'; mode = 'trigger' }
    primaryLeft    = @{ path = 'input/square'; mode = 'button' }
    primaryRight   = @{ path = 'input/cross'; mode = 'button' }
    secondaryLeft  = @{ path = 'input/triangle'; mode = 'button' }
    secondaryRight = @{ path = 'input/circle'; mode = 'button' }
    menuRight      = @{ path = 'input/options'; mode = 'button' }
    createLeft     = @{ path = 'input/create'; mode = 'button' }
}

# ---------------------------------------------------------------------------
# Ordered binding rows per action set. Hands are explicit so the emitted
# files keep their historical order; mirrored sets spell out the swapped
# layout exactly as shipped. Behaviors map SteamVR input events to action
# name suffixes ('global/in/...' targets another set verbatim).
# ---------------------------------------------------------------------------

function New-Row([string]$hand, [string]$inputName, [hashtable]$behaviors, [string]$mode = $null) {
    # NOTE: never name this '$input' - that identifier is the automatic
    # pipeline-enumerator variable and silently corrupts the returned row.
    @{ hand = $hand; input = $inputName; behaviors = $behaviors; mode = $mode }
}

$gameplayGeneric = @(
    (New-Row 'left'  'stickLeft'      @{ position = 'move'; click = 'sprint' }),
    (New-Row 'right' 'stickRight'     @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'quick_light' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'jump' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'notebook' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'holster' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'examine' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'interact' }),
    (New-Row 'right' 'gripRight'      @{ click = 'inventory' }),
    (New-Row 'left'  'systemLeft'     @{ click = 'pause' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$gameplayOculus = @(
    (New-Row 'left'  'stickLeft'      @{ position = 'move'; click = 'sprint' }),
    (New-Row 'right' 'stickRight'     @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'quick_light' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'jump' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'notebook' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'holster' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'examine' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'interact' }),
    (New-Row 'right' 'gripRight'      @{ click = 'inventory' }),
    (New-Row 'left'  'systemLeft'     @{ click = 'pause' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'recenterRight'  @{ click = 'global/in/recenter' })
)

$gameplayWmr = @(
    (New-Row 'left'  'stickLeft'      @{ position = 'move'; click = 'sprint' }),
    (New-Row 'right' 'stickRight'     @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'quick_light' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'jump' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'notebook' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'interact' }),
    (New-Row 'right' 'gripRight'      @{ click = 'inventory' }),
    (New-Row 'right' 'menuRight'      @{ click = 'examine' }),
    (New-Row 'right' 'trackpadRight'  @{ click = 'pause' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$gameplayVive = @(
    (New-Row 'left'  'stickLeft'      @{ position = 'move' } 'trackpad'),
    (New-Row 'right' 'stickRight'     @{ click = 'sprint' } 'trackpad'),
    (New-Row 'left'  'gripLeft'       @{ click = 'quick_light' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'jump' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'notebook' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'interact' }),
    (New-Row 'right' 'gripRight'      @{ click = 'inventory' }),
    (New-Row 'right' 'menuRight'      @{ click = 'examine' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'menuRight'      @{ click = 'global/in/recenter' })
)

$gameplayPsvr2 = @(
    (New-Row 'left'  'stickLeft'      @{ position = 'move'; click = 'sprint' }),
    (New-Row 'right' 'stickRight'     @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'quick_light' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'notebook' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'jump' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'interact' }),
    (New-Row 'right' 'gripRight'      @{ click = 'inventory' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'holster' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'examine' }),
    (New-Row 'right' 'menuRight'      @{ click = 'pause' })
)

$uiGeneric = @(
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'right' 'stickRight'     @{ click = 'drag' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'back' }),
    (New-Row 'right' 'gripRight'      @{ click = 'close' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'close' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$uiOculus = @(
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'right' 'stickRight'     @{ click = 'drag' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'back' }),
    (New-Row 'right' 'gripRight'      @{ click = 'close' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'close' })
)

$uiWmr = @(
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'right' 'stickRight'     @{ click = 'drag' }),
    (New-Row 'right' 'menuRight'      @{ click = 'back' }),
    (New-Row 'right' 'gripRight'      @{ click = 'close' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'close' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$uiVive = @(
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'right' 'stickRight'     @{ click = 'drag' } 'trackpad'),
    (New-Row 'right' 'menuRight'      @{ click = 'back' }),
    (New-Row 'right' 'gripRight'      @{ click = 'close' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'close' })
)

$uiPsvr2 = @(
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'right' 'stickRight'     @{ click = 'drag' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'back' }),
    (New-Row 'right' 'gripRight'      @{ click = 'close' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'close' }),
    (New-Row 'right' 'menuRight'      @{ click = 'close' })
)

# Mirrored sets: hands swapped relative to the base sets, in the exact order
# the reviewed files ship.

$gameplayGenericLeft = @(
    (New-Row 'right' 'stickRight'     @{ position = 'move'; click = 'sprint' }),
    (New-Row 'left'  'stickLeft'      @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'quick_light' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'jump' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'notebook' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'holster' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'examine' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'interact' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'inventory' }),
    (New-Row 'left'  'systemLeft'     @{ click = 'pause' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'trackpadRight'  @{ click = 'global/in/recenter' })
)

$gameplayOculusLeft = @(
    (New-Row 'right' 'stickRight'     @{ position = 'move'; click = 'sprint' }),
    (New-Row 'left'  'stickLeft'      @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'right' 'gripRight'      @{ click = 'quick_light' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'jump' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'notebook' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'holster' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'examine' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'interact' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'inventory' }),
    (New-Row 'left'  'systemLeft'     @{ click = 'pause' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'recenterRight'  @{ click = 'global/in/recenter' })
)

$gameplayWmrLeft = @(
    (New-Row 'right' 'stickRight'     @{ position = 'move'; click = 'sprint' }),
    (New-Row 'left'  'stickLeft'      @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'right' 'gripRight'      @{ click = 'quick_light' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'jump' }),
    (New-Row 'right' 'menuRight'      @{ click = 'notebook' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'interact' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'inventory' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'examine' }),
    (New-Row 'right' 'trackpadRight'  @{ click = 'pause' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'trackpadRight'  @{ click = 'global/in/recenter' })
)

$gameplayViveLeft = @(
    (New-Row 'right' 'stickRight'     @{ position = 'move' } 'trackpad'),
    (New-Row 'left'  'stickLeft'      @{ click = 'sprint' } 'trackpad'),
    (New-Row 'right' 'gripRight'      @{ click = 'quick_light' }),
    (New-Row 'right' 'triggerRight'   @{ click = 'jump' }),
    (New-Row 'right' 'menuRight'      @{ click = 'notebook' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'interact' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'inventory' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'examine' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'global/in/recenter' }),
    (New-Row 'right' 'menuRight'      @{ click = 'global/in/recenter' })
)

$gameplayPsvr2Left = @(
    (New-Row 'right' 'stickRight'     @{ position = 'move'; click = 'sprint' }),
    (New-Row 'left'  'stickLeft'      @{ position = 'turn'; click = 'crouch' }),
    (New-Row 'right' 'gripRight'      @{ click = 'quick_light' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'notebook' }),
    (New-Row 'left'  'primaryLeft'    @{ click = 'jump' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'interact' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'inventory' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'holster' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'examine' }),
    (New-Row 'right' 'menuRight'      @{ click = 'pause' })
)

$uiGenericLeft = @(
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'left'  'stickLeft'      @{ click = 'drag' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'back' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'close' }),
    (New-Row 'right' 'secondaryRight' @{ click = 'close' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$uiOculusLeft = @(
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'left'  'stickLeft'      @{ click = 'drag' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'back' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'close' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'close' })
)

$uiWmrLeft = @(
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'left'  'stickLeft'      @{ click = 'drag' }),
    (New-Row 'left'  'menuLeft'       @{ click = 'back' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'close' }),
    (New-Row 'right' 'menuRight'      @{ click = 'close' }),
    (New-Row 'left'  'trackpadLeft'   @{ click = 'global/in/recenter' })
)

$uiViveLeft = @(
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'left'  'stickLeft'      @{ click = 'drag' } 'trackpad'),
    (New-Row 'left'  'menuLeft'       @{ click = 'back' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'close' }),
    (New-Row 'right' 'menuRight'      @{ click = 'close' })
)

$uiPsvr2Left = @(
    (New-Row 'right' 'triggerRight'   @{ click = 'select' }),
    (New-Row 'left'  'triggerLeft'    @{ click = 'select' }),
    (New-Row 'left'  'stickLeft'      @{ click = 'drag' }),
    (New-Row 'left'  'secondaryLeft'  @{ click = 'back' }),
    (New-Row 'left'  'gripLeft'       @{ click = 'close' }),
    (New-Row 'right' 'primaryRight'   @{ click = 'close' }),
    (New-Row 'right' 'menuRight'      @{ click = 'close' })
)

$families = @{
    generic = @{
        Gameplay     = $gameplayGeneric
        GameplayLeft = $gameplayGenericLeft
        Ui           = $uiGeneric
        UiLeft       = $uiGenericLeft
        OffhandTriggerLeft  = 'triggerLeft'
        OffhandTriggerRight = 'triggerRight'
    }
    oculus = @{
        Gameplay     = $gameplayOculus
        GameplayLeft = $gameplayOculusLeft
        Ui           = $uiOculus
        UiLeft       = $uiOculusLeft
        OffhandTriggerLeft  = 'triggerLeft'
        OffhandTriggerRight = 'triggerRight'
    }
    wmr = @{
        Gameplay     = $gameplayWmr
        GameplayLeft = $gameplayWmrLeft
        Ui           = $uiWmr
        UiLeft       = $uiWmrLeft
        OffhandTriggerLeft  = 'triggerLeft'
        OffhandTriggerRight = 'triggerRight'
    }
    vive = @{
        Gameplay     = $gameplayVive
        GameplayLeft = $gameplayViveLeft
        Ui           = $uiVive
        UiLeft       = $uiViveLeft
        OffhandTriggerLeft  = 'triggerLeft'
        OffhandTriggerRight = 'triggerRight'
    }
    psvr2 = @{
        Gameplay     = $gameplayPsvr2
        GameplayLeft = $gameplayPsvr2Left
        Ui           = $uiPsvr2
        UiLeft       = $uiPsvr2Left
        OffhandTriggerLeft  = 'triggerLeft'
        OffhandTriggerRight = 'triggerRight'
    }
}

$controllers = @(
    (New-Controller -FileName 'knuckles.json' -ControllerType 'knuckles' `
        -Name 'Penumbra VR - Valve Index' `
        -Description 'Default Valve Index bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $true -FamilyKey 'generic'  -InputMap $inputGeneric),
    (New-Controller -FileName 'oculus_touch.json' -ControllerType 'oculus_touch' `
        -Name 'Penumbra VR - Touch' `
        -Description 'Default Meta Quest and Oculus Rift Touch bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $true -FamilyKey 'oculus'  -InputMap $inputOculus),
    (New-Controller -FileName 'pico4_controller.json' -ControllerType 'pico4_controller' `
        -Name 'Penumbra VR - Pico 4' `
        -Description 'Default Pico 4 and Pico 4 Ultra bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $true -FamilyKey 'oculus'  -InputMap $inputOculus),
    (New-Controller -FileName 'pico_neo3_controller.json' -ControllerType 'pico_neo3_controller' `
        -Name 'Penumbra VR - Pico Neo 3' `
        -Description 'Default Pico Neo 3 bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $true -FamilyKey 'oculus'  -InputMap $inputOculus),
    (New-Controller -FileName 'microsoft_motion_controller.json' -ControllerType 'microsoft/motion_controller' `
        -Name 'Penumbra VR - Windows Mixed Reality' `
        -Description 'Default Windows Mixed Reality bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $false -FamilyKey 'wmr'  -InputMap $inputWmr),
    (New-Controller -FileName 'holographic_controller.json' -ControllerType 'holographic_controller' `
        -Name 'Penumbra VR - Windows Mixed Reality (holographic)' `
        -Description 'Default Windows Mixed Reality (holographic) bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/grip' -Skeletal $false -FamilyKey 'wmr'  -InputMap $inputWmr),
    (New-Controller -FileName 'vive_controller.json' -ControllerType 'vive_controller' `
        -Name 'Penumbra VR - HTC Vive' `
        -Description 'Legacy-compatible HTC Vive bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/raw' -Skeletal $false -FamilyKey 'vive'  -InputMap $inputVive),
    (New-Controller -FileName 'psvr2_sense.json' -ControllerType 'playstation_vr2_sense' `
        -Name 'Penumbra VR - PS VR2 Sense' `
        -Description 'Default PS VR2 Sense bindings for Penumbra: Overture VR Rework' `
        -GripPose 'pose/handgrip' -Skeletal $true -FamilyKey 'psvr2'  -InputMap $inputPsvr2)
)

# ---------------------------------------------------------------------------
# Emission
# ---------------------------------------------------------------------------

function Get-SourceObject {
    param([pscustomobject]$controller, [hashtable]$family, [string]$setName, [hashtable]$row)

    $physical = $controller.Input[$row.input]
    if ($null -eq $physical) {
        throw ("Binding row references unknown input '{0}' ({1})." -f $row.input, $controller.ControllerType)
    }

    $inputs = [ordered]::new()
    foreach ($behavior in ($row.behaviors.Keys | Sort-Object)) {
        $target = $row.behaviors[$behavior]
        if ($target.StartsWith('global/in/')) {
            # Cross-set reference into the global action set.
            $target = '/actions/' + $target
        }
        elseif (-not $target.StartsWith('/')) {
            $target = '/actions/' + $setName + '/in/' + $target
        }
        $inputs[$behavior] = [ordered]@{ output = $target }
    }

    $mode = if ($row.mode) { $row.mode } else { $physical.mode }
    return [ordered]@{
        inputs = $inputs
        mode   = $mode
        path   = '/user/hand/' + $row.hand + '/' + $physical.path
    }
}

function Get-BindingDocument {
    param([pscustomobject]$controller)

    $family = $families[$controller.Family]
    $nl = [Environment]::NewLine

    $sets = [ordered]::new()

    # Global set: haptics, poses, optional skeleton, optional recenter source.
    # Poses are grouped by type exactly as SteamVR's own samples ship them:
    # both grip poses first, then both aim poses.
    $poses = @()
    foreach ($hand in @('left', 'right')) {
        $poses += [ordered]@{
            output = "/actions/global/in/${hand}_pose"
            path   = "/user/hand/$hand/" + $controller.GripPose
        }
    }
    foreach ($hand in @('left', 'right')) {
        $poses += [ordered]@{
            output = "/actions/global/in/${hand}_aim"
            path   = "/user/hand/$hand/pose/aim"
        }
    }
    $skeleton = @()
    if ($controller.Skeletal) {
        foreach ($hand in @('left', 'right')) {
            $skeleton += [ordered]@{
                output = "/actions/global/in/${hand}_skeleton"
                path   = "/user/hand/$hand/input/skeleton/$hand"
            }
        }
    }
    $globalSources = @()
    if ($controller.ControllerType -eq 'playstation_vr2_sense') {
        $recenterRow = New-Row 'left' 'createLeft' @{ click = 'global/in/recenter' }
        $globalSources += Get-SourceObject $controller $family 'global' $recenterRow
    }
    $sets['/actions/global'] = [ordered]@{
        chords   = @()
        haptics  = @(
            [ordered]@{ output = '/actions/global/out/left_haptic'; path = '/user/hand/left/output/haptic' },
            [ordered]@{ output = '/actions/global/out/right_haptic'; path = '/user/hand/right/output/haptic' }
        )
        poses    = $poses
        skeleton = $skeleton
        sources  = $globalSources
    }

    # Shared offhand set: both hands' triggers reach the off-hand interact.
    # Rows are materialized through the same loop pattern as every other set:
    # a comma after a parenthesized argument in command position glues the
    # following pipeline into one array and corrupts parameter binding.
    $offhandRows = @(
        (New-Row 'left'  'triggerLeft'  @{ click = 'interact' }),
        (New-Row 'right' 'triggerRight' @{ click = 'interact' })
    )
    $offhandSources = foreach ($row in $offhandRows) {
        Get-SourceObject $controller $family 'offhand' $row
    }
    $sets['/actions/offhand'] = [ordered]@{
        chords   = @()
        haptics  = @()
        poses    = @()
        skeleton = @()
        sources  = @($offhandSources)
    }

    foreach ($entry in @(
        @{ key = '/actions/gameplay'; rows = $family.Gameplay },
        @{ key = '/actions/ui'; rows = $family.Ui },
        @{ key = '/actions/gameplay_left'; rows = $family.GameplayLeft },
        @{ key = '/actions/ui_left'; rows = $family.UiLeft }
    )) {
        $setName = $entry.key.TrimStart('/').Split('/')[1]
        $sources = foreach ($row in $entry.rows) {
            Get-SourceObject $controller $family $setName $row
        }
        $sets[$entry.key] = [ordered]@{
            chords   = @()
            haptics  = @()
            poses    = @()
            skeleton = @()
            sources  = @($sources)
        }
    }

    return [ordered]@{
        action_manifest_version = 0
        alias_info              = [ordered]@{}
        bindings                = $sets
        controller_type         = $controller.ControllerType
        description             = $controller.Description
        name                    = $controller.Name
        options                 = [ordered]@{}
        simulated_actions       = @()
    }
}

function ConvertTo-JsonText($value, [int]$indent = 0) {
    $pad = ' ' * $indent
    $childPad = ' ' * ($indent + 2)

    if ($value -is [System.Collections.IDictionary]) {
        if ($value.Count -eq 0) { return '{}' }
        $lines = foreach ($key in $value.Keys) {
            $childPad + '"' + $key + '": ' + (ConvertTo-JsonText $value[$key] ($indent + 2))
        }
        return "{`n" + ($lines -join ",`n") + "`n$pad}"
    }
    if ($value -is [System.Collections.IEnumerable] -and $value -isnot [string]) {
        $items = @($value)
        if ($items.Count -eq 0) { return '[]' }
        $lines = foreach ($item in $items) {
            $childPad + (ConvertTo-JsonText $item ($indent + 2))
        }
        return "[`n" + ($lines -join ",`n") + "`n$pad]"
    }
    if ($value -is [bool]) { return $(if ($value) { 'true' } else { 'false' }) }
    if ($value -is [int] -or $value -is [long] -or $value -is [double]) { return "$value" }
    return '"' + ($value -replace '\\', '\\\\' -replace '"', '\"') + '"'
}

# ---------------------------------------------------------------------------
# Verification helpers
# ---------------------------------------------------------------------------

function Test-JsonEqual($a, $b) {
    if ($null -eq $a -or $null -eq $b) { return ($null -eq $a -and $null -eq $b) }
    if ($a -is [pscustomobject]) {
        if ($b -isnot [pscustomobject]) { return $false }
        $propsA = @($a.PSObject.Properties | Sort-Object Name)
        $propsB = @($b.PSObject.Properties | Sort-Object Name)
        if ($propsA.Count -ne $propsB.Count) { return $false }
        for ($i = 0; $i -lt $propsA.Count; ++$i) {
            if ($propsA[$i].Name -ne $propsB[$i].Name) { return $false }
            if (-not (Test-JsonEqual $propsA[$i].Value $propsB[$i].Value)) { return $false }
        }
        return $true
    }
    if ($a -is [System.Collections.IEnumerable] -and $a -isnot [string]) {
        if ($b -isnot [System.Collections.IEnumerable] -or $b -is [string]) { return $false }
        $arrayA = @($a); $arrayB = @($b)
        if ($arrayA.Count -ne $arrayB.Count) { return $false }
        for ($i = 0; $i -lt $arrayA.Count; ++$i) {
            if (-not (Test-JsonEqual $arrayA[$i] $arrayB[$i])) { return $false }
        }
        return $true
    }
    return "$a" -eq "$b"
}

# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

$results = foreach ($controller in $controllers) {
    $document = Get-BindingDocument $controller
    $json = (ConvertTo-JsonText $document) -replace "`n", "`r`n"

    $targetPath = Join-Path $OutputRoot $controller.FileName
    $existing = if (Test-Path -LiteralPath $targetPath) { Get-Content -Raw -LiteralPath $targetPath } else { $null }

    if ($Check) {
        if ($null -eq $existing) {
            [pscustomobject]@{ File = $controller.FileName; Status = 'MISSING' }
            continue
        }
        $equal = Test-JsonEqual (ConvertFrom-Json $json) (ConvertFrom-Json $existing)
        [pscustomobject]@{ File = $controller.FileName; Status = $(if ($equal) { 'IN SYNC' } else { 'DRIFTED' }) }
    }
    else {
        if ($existing -ne $json) {
            Set-Content -LiteralPath $targetPath -Value $json -Encoding ascii -NoNewline
            [pscustomobject]@{ File = $controller.FileName; Status = 'REWRITTEN' }
        }
        else {
            [pscustomobject]@{ File = $controller.FileName; Status = 'unchanged' }
        }
    }
}

$results | Format-Table -AutoSize

if ($Check -and ($results | Where-Object Status -ne 'IN SYNC')) {
    throw 'Generated bindings do not match the files on disk; run ./scripts/generate-bindings.ps1 or revert the manual edits.'
}
