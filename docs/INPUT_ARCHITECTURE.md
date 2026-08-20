# Input architecture boundary

Penumbra VR currently has two input systems that meet in
`PenumbraOverture/ButtonHandler.cpp`. SteamVR controller actions do not replace
HPL1's keyboard and mouse action system; both paths are evaluated so desktop
fallback and existing game scripts continue to work.

## Runtime and HPL1 responsibilities

| Responsibility | Current owner | Main implementation |
| --- | --- | --- |
| Load the SteamVR action manifest | HPL1 | `HPL1Engine/sources/input/SteamVRInput.cpp` |
| Read logical left/right poses and pose validity | HPL1 | `SteamVRInput.cpp`, `TrackedController.cpp` |
| Expose the dominant-hand preference and gate poses, legacy fallback, and mirrored action sets by it | HPL1 | `Game.cpp` (`vr_dominant_hand`), `SteamVRInput.cpp` (`SetHandedness`), `VRHelper.hpp` |
| Poll compositor poses and keep a legacy device-role fallback | HPL1 | `HPL1Engine/sources/game/Game.cpp` |
| Read SteamVR digital/analog actions | HPL1 | `SteamVRInput.cpp` |
| Read legacy OpenVR controller IDs and axes | HPL1 | `TrackedController.cpp` |
| Normalize both controller paths into intent-named state | HPL1 | `cVRInputState`, `SteamVRInput.cpp` |
| Read keyboard and mouse actions | HPL1 | `HPL1Engine/sources/input/` and `cInput` |
| Convert tracking poses into Penumbra world space | HPL1 | `HPL1Engine/include/game/VRTracking.h` |
| Define physical controller bindings | Data/runtime | `data/vr/actions.json`, `data/vr/bindings/*.json` |

HPL1 should expose device-independent facts: a logical hand pose is valid, an
action changed state, an analog axis has a value, a haptic request should be
sent, or a hand is the configured dominant one. It should not decide what a
door, key, notebook, weapon, or inventory item does. Handedness is therefore a
semantic preference (`cGame::vr_dominant_hand`) consulted by gameplay and UI;
`cSteamVRInput::SetHandedness` applies it to pose gating, the legacy fallback
buttons, and the choice of action set.

## Mirrored action sets

SteamVR bindings are mirrored rather than swapped in code: `data/vr/actions.json`
declares `/actions/gameplay_left` and `/actions/ui_left` with the same
intent-named actions as `/actions/gameplay` and `/actions/ui`, and every binding
JSON carries a `_left` set whose controls are the exact left/right mirror of the
default set (PS VR2: move on the right stick and turn on the left stick, sprint
and crouch swapped, notebook/holster on the off hand's cross/circle, quick
light on the off hand's R1, interact/inventory/examine/jump on the dominant
hand's L2/L1/triangle/square, UI select on L2+R2, UI drag on the left stick,
UI back on the dominant triangle, UI close on L1/cross/options). Pause and
recenter are deliberately NOT mirrored: they stay on the same physical
controls as the default layout (Options and left Create, or the equivalent
per profile), because sharing a physical input between two actions of the
same hand set proved unreliable in SteamVR. The global set (poses, haptics,
skeletons) is not mirrored because those actions are physical and
hand-agnostic.

The PS VR2 and Touch drivers expose asymmetric face buttons (PS VR2 left:
square/triangle, right: cross/circle; Touch left: x/y, right: a/b), so each
mirrored binding moves an action to the same physical button position on the
other hand instead of reusing the same input name. Changing the dominant hand
while a tool or light is attached returns the player to bare hands first
(`cInit::ApplyVRSettings`), so the old slot models cannot duplicate on the
mirrored slots. The notebook does the same when it opens (`cNotebook::SetActive`):
it is a navigation-only view that always hangs on the off hand, so both hands
return to bare (holstered tools, lights off). The notebook is an 800x600 px
UI surface whose origin is anchored to the off hand, but the visible book
(350x460 px) is drawn at (225,70) inside it, so the placement targets the
book rectangle instead of the surface: the hand goes to the book's outer
edge (UI x=225 for the left off hand, x=575 for the right off hand, because
in this engine the same local +X points toward the body only on the left
hand) and the book extends about a book width (0.24 m at 1/1450) toward the
body centre in both dominant modes. This geometry lesson (anchor to the
visible object, not the surface origin) is what a future equipped-object
hold needs so the hand is seen holding the item, and to animate the grab
motion. Grab-target detection
runs for the dominant hand only (`cPlayerState_Normal_VR`), so the off hand
never highlights or grabs anything.

`cSteamVRInput` resolves both sets once at startup. `Update` activates the set
that matches `mHandedness` (plus the global set), reads every action through
`ActionFor` so consumer code never sees a hand, and treats a handedness change
like a context change: edges are suppressed for one frame so a held button
cannot fire on the mirrored action it was swapped onto. `UpdateLegacyState`
mirrors the same layout for the OpenVR polling fallback by reading the
dominant-hand state where the default layout used the right hand and the
off-hand state where it used the left hand.

Recenter is the one action that receives special handling in `Update`. It is a
global action read hand-agnostically before the context branch, and its edge
latch applies only when the dominant hand changes. A context switch or a return
from the legacy fallback must not swallow a real create press, because the
fallback path has no create button to poll (the PS VR2 legacy binding exposes
no create source at all). Recenter also counts as an active action on every
frame: the player stands still to re-aim, so a recenter press is often the only
active input that frame, and an idle frame that dropped to legacy polling
silently lost the press (observed as recenter working in one dominant mode and
not the other, depending on whether another action happened to be active).

## PenumbraVR responsibilities

| Responsibility | Main implementation |
| --- | --- |
| Select gameplay or UI action context | `PenumbraOverture/ButtonHandler.cpp` |
| Translate controller state into player commands | `ButtonHandler.cpp` |
| Decide whether R2 interacts, attacks, drags, selects, or uses an item | `ButtonHandler.cpp` and player-state classes |
| Resolve grab, move, push, throw, door, and tool behaviour | `PlayerState_Interact_VR.cpp`, `PlayerState_Weapon_VR.cpp`, related entity classes |
| Own independent left/right target and held-body state | `Player.h`, `PlayerState_Misc_VR.cpp`, `PlayerState_Interact_VR.cpp` |
| Place the inventory, notebook, and menu pointer | `Inventory.cpp`, `Notebook.cpp`, `MainMenu.cpp`, `NumericalPanel.cpp` |
| Render equipped hand-held models | `PlayerHands.cpp` |
| Animate the rigged HUD hand models from the skeletal summary (grip/trigger pose tracks blended by finger curl) | `PlayerHands.cpp` |
| Translate semantic feedback events into controller vibration profiles | `VRHaptics.cpp` |

Penumbra owns gameplay meaning. Changing how a key is applied to a locked door,
for example, belongs here rather than in the SteamVR backend.

## Current event flow

```text
SteamVR controller profile
        |
        v
SteamVR binding JSON
        |
        v
SteamVR actions ---------> cSteamVRInput <--------- legacy OpenVR polling
        |                  cVRInputState                  |
        |          intents, movement, input edges         |
        |                         ^                       |
        +------ pose actions      |       TrackedController
                                  |
                                  v
cButtonHandler <---------------- HPL1 keyboard/mouse cInput
        |
        +--> player commands and player-state transitions
        +--> inventory/notebook/menu mouse-compatible callbacks
        +--> pause, close, holster, light, and notebook commands

Penumbra gameplay event --> cVRHaptics profile --> cSteamVRInput output action
                                                    |
                                                    +--> logical left/right hand
```

## Haptic output

Gameplay code emits intent-named events rather than OpenVR vibration values.
`cVRHaptics` owns the duration, frequency, and amplitude profiles for UI
selection, object pickup/drop, successful item use, light toggles, melee
impacts, and player damage.
It then routes the result to the logical left hand, right hand, or both through
HPL1's existing SteamVR vibration actions. Keeping the profiles in one place
allows hardware tuning without changing gameplay state machines. Each profile
also owns its minimum repeat interval so noisy physical contacts cannot flood a
controller with overlapping pulses.

## Legacy compatibility boundary

`TrackedController::ButtonState` retains Vive-era field names only for raw
OpenVR fallback polling. `cSteamVRInput::UpdateLegacyState` translates those
fields at the boundary into the same intent-named `cVRInputState` produced by
SteamVR actions. Penumbra gameplay consumes `interact`, `inventory`, `sprint`,
`uiSelect`, and the other intents without inferring their physical controls.

SteamVR actions are no longer copied into `TrackedController::ButtonState`.
The tracked-controller class is responsible for pose, velocity, validity, and
raw fallback input; binding JSON is responsible for physical layout. Menu code
still deliberately calls the original mouse-oriented callbacks so the mature
flat-game widget and inventory logic can be reused.

## Safe modernization scope

Changes that can remain inside HPL1 without rewriting interaction mechanics:

- additional SteamVR controller bindings;
- pose acquisition and reconnection;
- dead-zone and analog normalization;
- action-set lifecycle and input-edge handling;
- pose gating and legacy fallback selection by the dominant hand;
- centralized haptic output;
- tracking-to-world, recenter, and turn transforms.

Changes that require Penumbra gameplay work:

- using keys and inventory items on world entities;
- one-hand or two-hand grabbing;
- door and drawer manipulation;
- melee/tool semantics;
- inventory context behaviour;
- notebook and diegetic UI interaction;
- crouch state and character collider changes.

With controller transport normalized, the next input work can add dead zones,
more controller bindings, or further semantic preferences at one stable
boundary without rewriting HPL1's keyboard/mouse system or Penumbra's
interaction state machines.

The persistent `[VR]` settings use this boundary directly. HPL1 applies a
radial movement dead zone to both action and fallback axes, Penumbra applies the
movement-speed multiplier, and `cVRTrackingSpace` owns the vertical calibration
offset (posture offset plus the seated-play-mode offset). Defaults preserve the
previously validated behaviour apart from the small anti-drift dead zone.

`Options → VR Settings` edits these values through `cVRSettings`, applies them
to the live engine boundary, and saves the normal user config immediately. R2
increments or selects the next value, R3 decrements or selects the previous
value, and Circle is reserved for back navigation in the main menu. The
dominant-hand, play-mode, and player-height rows sit at the top of the list;
the player-height row carries a Calibrate button that measures the HMD height
directly.

World yaw is also owned by `cVRTrackingSpace`. Snap turn, smooth turn, and
orientation recenter modify that single value, so the HMD, both hands, physical
head displacement, held objects, and world-space UI all cross the same rigid
transform. Turning pivots around the current HMD anchor rather than the room
origin. Raw tracking-space direction deltas are rotated into world space
without inheriting positional or height translation.

## Hand animation boundary

The skeletal summary follows the same boundary as the poses: `cSteamVRInput`
reads the device skeleton (with an animation-layer fallback for drivers that
only expose estimated data) and stores one normalized grip/trigger pair per
hand on the tracked-hand state. `PlayerHands.cpp` consumes only that summary:
it builds two rotation-only pose tracks per hand (HandGrab, HandTrigger) at rig
setup and blends their weights by grip and trigger. The hand animation is
experimental: joints, angles, and axes were retuned from bind geometry in
August 2026 and are frozen for now; see `docs/ROADMAP.md` for the known
limitations.

## Hand interaction state

Penumbra now records target and held-body ownership separately for the left and
right hands. Target detection may run for both hands, but the currently exposed
R2 interaction deliberately mirrors only the configured dominant-hand target
into the legacy `PickedBody` slot used by original entity callbacks. This
removes the old first-hand-wins behaviour without prematurely enabling L2
interaction; changing the dominant hand in VR Settings reselects which hand
drives interaction, pointer, throw velocity, and haptics.

The immutable overlap shape is allocated once per loaded physics world and
reused by both sequential hand queries. Invalid hand poses are skipped, and an
entity being destroyed clears any matching hand target or held-body reference.

## Validated lifecycle

The clean Release milestone was exercised on PS VR2 Sense hardware through
Steam launch, tutorial, new game, continue, save restoration, and repeated
gameplay/UI transitions. A lost action pose hides and releases that hand; the
other hand remains available for pointing, and SteamVR can reacquire the pose
without retaining a stale tracked-device index. The same validation covered
inventory pointing, action selection, crouch, and locked-door collision. The
bundled Vive mapping remains a compatibility profile and still needs a
Vive-specific hardware pass.

This describes the 14 August 2026 milestone build. The crouch and hand-rig
areas changed afterwards (16–18 August 2026). The crouch and height system has
been revalidated and is confirmed correct; the hand animation remains
experimental and currently shows incorrect visual behaviour. Dominant-hand,
seated/standing play mode, and player-height settings were added on
18 August 2026 and await hardware validation. The left-handed mirror
(equipment slots bound to the dominant/off hand, mirrored SteamVR action sets
and legacy fallback, notebook always on the off hand with its lateral
placement targeted at the visible book rectangle (hand at the book's outer
edge, book extending toward the body in both modes), interact target
detection accepting the left rig name
`LeftHand` and running for the dominant hand only, asymmetric PS VR2/Touch
face-button mirrors, per-profile pause and recenter shared between modes, and
bare-hands resets on handedness change and notebook open) was added on
19 August 2026 and awaits a final PS VR2 regression pass.
