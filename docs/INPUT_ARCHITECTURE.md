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
| Poll compositor poses and keep a legacy device-role fallback | HPL1 | `HPL1Engine/sources/game/Game.cpp` |
| Read SteamVR digital/analog actions | HPL1 | `SteamVRInput.cpp` |
| Read legacy OpenVR controller IDs and axes | HPL1 | `TrackedController.cpp` |
| Read keyboard and mouse actions | HPL1 | `HPL1Engine/sources/input/` and `cInput` |
| Convert tracking poses into Penumbra world space | HPL1 | `HPL1Engine/include/game/VRTracking.h` |
| Define physical controller bindings | Data/runtime | `data/vr/actions.json`, `data/vr/bindings/*.json` |

HPL1 should expose device-independent facts: a logical hand pose is valid, an
action changed state, an analog axis has a value, or a haptic request should be
sent. It should not decide what a door, key, notebook, weapon, or inventory item
does.

## PenumbraVR responsibilities

| Responsibility | Main implementation |
| --- | --- |
| Select gameplay or UI action context | `PenumbraOverture/ButtonHandler.cpp` |
| Translate controller state into player commands | `ButtonHandler.cpp` |
| Decide whether R2 interacts, attacks, drags, selects, or uses an item | `ButtonHandler.cpp` and player-state classes |
| Resolve grab, move, push, throw, door, and tool behaviour | `PlayerState_Interact_VR.cpp`, `PlayerState_Weapon_VR.cpp`, related entity classes |
| Place the inventory, notebook, and menu pointer | `Inventory.cpp`, `Notebook.cpp`, `MainMenu.cpp`, `NumericalPanel.cpp` |
| Render equipped hand-held models | `PlayerHands.cpp` |

Penumbra owns gameplay meaning. Changing how a key is applied to a locked door,
for example, belongs here rather than in the SteamVR backend.

## Current event flow

```text
PS VR2 Sense / Vive
        |
        v
SteamVR binding JSON
        |
        v
cSteamVRInput --------------------------+
  action sets, poses, buttons, axes      |
        |                                | fallback
        v                                v
TrackedController <------------- legacy OpenVR polling
        |
        v
cButtonHandler <---------------- HPL1 keyboard/mouse cInput
        |
        +--> player commands and player-state transitions
        +--> inventory/notebook/menu mouse-compatible callbacks
        +--> pause, close, holster, light, and notebook commands
```

## Important legacy coupling

`TrackedController::ButtonState` still uses Vive-era field names as a temporary
transport. For example, a PS VR2 action may be stored in `gripPressed` or
`padPressed` even when its physical source is R1 or L3. Gameplay code therefore
must not infer the physical control from those member names.

Locomotion also reads the left analog values directly from the tracked
controller state, while most command-style actions are dispatched by
`cButtonHandler`. Menu code deliberately calls the original mouse-oriented
callbacks so the mature flat-game widget and inventory logic can be reused.

This compatibility layer is useful, but it is the next boundary to improve. A
future `VRHandInputState` should name actions by intent (`Interact`, `Inventory`,
`Sprint`, `UISelect`) and leave `TrackedController` responsible only for pose,
velocity, validity, and legacy fallback.

## Safe modernization scope

Changes that can remain inside HPL1 without rewriting interaction mechanics:

- additional SteamVR controller bindings;
- pose acquisition and reconnection;
- dead-zone and analog normalization;
- action-set lifecycle and input-edge handling;
- handedness routing at the action level;
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

The practical next refactor is therefore not a new controller layout. It is to
replace the overloaded Vive-era `ButtonState` fields with intent-named VR input
state while preserving the existing `cButtonHandler` entry point. That gives
future control changes one stable boundary without forcing a rewrite of HPL1's
keyboard/mouse system or Penumbra's interaction state machines.
