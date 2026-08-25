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
per profile). SteamVR does not reliably support sharing one physical input
between two actions in the same hand set. The global set (poses, haptics,
skeletons) is not mirrored because those actions are physical and
hand-agnostic.

Every bundled profile binds both physical triggers (PS VR2: L2/R2; all other families: the index triggers) to `/actions/offhand/in/interact` in a
small dedicated action set. At runtime `cSteamVRInput` restricts that set to the
physical controller opposite the configured dominant hand, so the off-hand
trigger can report
off-hand interaction without activating the rest of the mirrored gameplay
layout. The input state records which controller produced the press edge and
keeps raw trigger polling only as a compatibility fallback for profiles that do
not yet bind the dedicated action.

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
body centre in both dominant modes. The same geometry rule now applies to
physics grabs: the pickup transform is measured against the rendered hand-rig
pose and the actual contact point is placed in the palm. Grab-target detection
runs for both hands in `cPlayerState_Normal_VR`; an off hand carrying an
attachment remains excluded so interaction cannot compete with a light or tool.

`cSteamVRInput` resolves the mirrored, global, and off-hand sets once at startup.
`Update` activates the gameplay/UI set that matches `mHandedness`, the global
set, and—during gameplay—the off-hand set restricted to the opposite physical
controller. Intent actions still pass through `ActionFor`, while interaction
additionally exposes its source hand to gameplay. A handedness change is treated
like a context change: edges are suppressed for one frame so a held button
cannot fire on the action it was swapped onto. `UpdateLegacyState` mirrors the
same layout for the OpenVR polling fallback by reading the dominant-hand state
where the default layout used the right hand and the off-hand state where it
used the left hand.

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
- simultaneous two-object holding or two-point grabbing;
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

`cSteamVRInput` reads the device skeletal summary and falls back to SteamVR's
animation summary when a driver exposes only estimated data. The tracked-hand
state stores grip, trigger, and the five per-finger curl values without exposing
SteamVR calls to gameplay code.

`PlayerHands.cpp` consumes that summary. Devices with skeletal data drive each
finger independently; other devices synthesize a staggered closing sequence
from grip and trigger. A soft dead zone and exponential smoothing are applied
before rotation-only poses are written to the rig bones. The current mesh and
pose limits are presentation concerns tracked in the [roadmap](ROADMAP.md), not
part of the runtime input contract.

## Hand interaction state

Penumbra records target and held-body ownership separately for the left and
right hands. Target detection runs for both bare hands; the controller that
produces the interaction edge is preferred, and gameplay falls back to the other
hand only when the pressing hand has no target. Before invoking an original
entity callback, Penumbra mirrors the chosen target into the legacy `PickedBody`
slot and records explicit grab ownership. The grab state then follows that
hand's rendered palm pose and routes release velocity, visibility, and haptics
to the same controller.

The immutable overlap shape is allocated once per loaded physics world and
reused by both sequential hand queries. Invalid hand poses are skipped, and an
entity being destroyed clears any matching hand target or held-body reference.
An additional small sphere supplies impulse-based hand nudge for ordinary
dynamic objects and swing doors. Inventory items and already-held bodies are
excluded; grabbable props receive lower speed and delta-velocity limits. Because
this proxy applies impulses only to dynamic bodies, it does not stop the visible
hand against static walls.

Although ownership is stored per hand, Penumbra still has one global active
player-interaction state. A bare off hand can initiate and own the current grab,
but simultaneous two-object holds and two-hand constraints on one body require a
future state-machine change.

## Failure and recovery lifecycle

If the action manifest cannot initialize or mandatory handles cannot be
resolved, the runtime uses legacy OpenVR polling. During normal action-based
input, a brief period with no active action origins keeps the last good state
for 500 ms; only a longer dropout enters the legacy path. This avoids swallowing
held input while SteamVR refreshes bindings.

A lost action pose hides the affected hand, blocks its spatial selection, and
releases any held interaction. The other hand remains available for UI
pointing. When the pose returns, the logical hand reacquires it without keeping
a stale tracked-device index.

`hpl.log` records whether SteamVR actions or legacy polling are active, the
detected controller types, bundled-profile matches, and pose acquisition or
loss. Dated hardware coverage and regression results are kept in the
[release history](RELEASES.md) so this architecture document can remain
descriptive rather than historical.
