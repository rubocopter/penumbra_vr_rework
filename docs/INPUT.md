# Controls and VR settings

Penumbra VR uses SteamVR Input profiles so controls can be adapted to each controller family. You can inspect or customize the installed bindings through SteamVR's controller binding interface.

This guide covers the controls, bundled profiles, VR options, hardware compatibility, and player-facing diagnostics. For implementation boundaries and event flow, see [Input architecture](INPUT_ARCHITECTURE.md).

## In this guide

- [Control scheme](#control-scheme)
- [Controller profiles](#controller-profiles)
- [Left-handed controls](#left-handed-controls)
- [VR settings](#vr-settings)
- [Haptic feedback](#haptic-feedback)
- [Hardware compatibility](#hardware-compatibility)
- [Troubleshooting input](#troubleshooting-input)

## Control scheme

Each controller profile maps its physical controls to the same gameplay actions:

| Action | Gameplay meaning |
| --- | --- |
| Move | Analog locomotion relative to the view |
| Turn | Disabled, snap, or smooth horizontal turning |
| Sprint | Run while moving |
| Crouch | Toggle crouch in Button or Hybrid mode |
| Quick light | Toggle the currently available light source |
| Notebook | Open or close the notebook |
| Holster | Unequip the current tool or weapon so Interact can target the world |
| Jump | Jump when the current player state permits it |
| Interact / use | Grab, operate, confirm, or use the equipped tool or item |
| Inventory | Open or close the inventory |
| Examine / back | Examine in gameplay; open item actions or navigate back in UI |
| Pause | Open or close the pause menu |
| Recenter | Put forward-facing VR content in front of the current horizontal view |
| UI point | Aim a tracked-controller ray at menus, inventory, and panels |
| UI select | Activate the pointed control or inventory action |
| UI alternate | Decrease a VR setting or use the inventory drag/combine action |

The physical labels below describe the bundled profiles, not universal in-game prompts. Custom SteamVR profiles can remap these actions.

## Controller profiles

### PS VR2 Sense

#### Gameplay

| Control | Action |
| --- | --- |
| Left stick | Move |
| L3 | Sprint |
| Right stick | Snap or smooth turn |
| R3 | Toggle crouch |
| L1 | Toggle the current quick light |
| Square | Notebook |
| Triangle | Holster the equipped tool or weapon |
| Cross | Jump |
| R2 | Interact, or use the equipped tool or weapon |
| L2, while the off hand is bare | Interact with or grab a nearby object using the off hand |
| R1 | Inventory |
| Circle | Examine |
| Options | Pause menu |
| Create | Recenter horizontal view |

#### Menus and inventory

| Control | Action |
| --- | --- |
| Aim with the dominant-hand controller | Move the pointer |
| R2 | Select; increase a VR setting; perform the default inventory action |
| R3 | Decrease a VR setting; drag or combine inventory items |
| Circle | Go back; open an inventory item's actions |
| R1, Square, or Options | Close the current screen |

If the dominant-hand controller loses tracking, the other hand can take over the pointer and its trigger can select. The mouse also remains available.

When a tool such as the hammer is equipped, R2 operates that tool. Press Triangle to holster it before using R2 to interact with doors or other objects.

To use an inventory item on the world, open the inventory with R1, point at the item, and press R2. After the inventory closes, aim at the target from interaction range and press R2 again. Release a button before using that same button to close a screen it just opened.

### HTC Vive

The bundled Vive compatibility profile preserves the original layout:

| Control | Gameplay | Menus and inventory |
| --- | --- | --- |
| Left trackpad | Move | — |
| Right trackpad | Sprint | Drag/combine |
| Left grip | Quick light | — |
| Left trigger | Jump | — |
| Left menu | Notebook | — |
| Right trigger | Interact/use | Select |
| Right grip | Inventory | Close |
| Right menu | Examine | Back |

Turn, button crouch, holster, pause, and recenter are not assigned in this compatibility profile. They can be mapped through SteamVR.

### Valve Index

The bundled `knuckles` profile follows the PS VR2 scheme. Recenter and pause stay on the left trackpad and left system button in both handedness modes.

#### Gameplay

| Control | Action |
| --- | --- |
| Left stick | Move |
| L3 (left stick click) | Sprint |
| Right stick | Snap or smooth turn |
| R3 (right stick click) | Toggle crouch |
| Left trigger | Toggle the current quick light |
| Right A | Jump |
| Left B | Notebook |
| Left A | Holster the equipped tool or weapon |
| Right B | Examine |
| Right trigger | Interact, or use the equipped tool or weapon |
| Right grip | Inventory |
| Left system | Pause menu |
| Left trackpad click | Recenter horizontal view |

#### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 | Drag or combine |
| Right B | Go back; open an inventory item's actions |
| Right grip or Left B | Close the current screen |

### Meta Quest and Oculus Touch

The bundled `oculus_touch` profile covers Quest 2/3/Pro and Rift controllers connected through SteamVR Link. Touch has no spare Create-style button, so recenter is unassigned by default and can be mapped through SteamVR.

#### Gameplay

| Control | Action |
| --- | --- |
| Left stick | Move |
| L3 (left stick click) | Sprint |
| Right stick | Snap or smooth turn |
| R3 (right stick click) | Toggle crouch |
| Left grip | Toggle the current quick light |
| Right A | Jump |
| Left X | Notebook |
| Left Y | Holster the equipped tool or weapon |
| Right B | Examine |
| Right trigger | Interact, or use the equipped tool or weapon |
| Right grip | Inventory |
| Left system / menu | Pause menu |

#### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 | Drag or combine |
| Right B | Go back; open an inventory item's actions |
| Right grip or Left X | Close the current screen |

### Windows Mixed Reality

The `microsoft/motion_controller` and `holographic_controller` profiles use the joysticks for movement and turning and the trackpads for secondary actions.

#### Gameplay

| Control | Action |
| --- | --- |
| Left stick | Move |
| L3 (left stick click) | Sprint |
| Right stick | Snap or smooth turn |
| R3 (right stick click) | Toggle crouch |
| Left grip | Toggle the current quick light |
| Left trigger | Jump |
| Left menu | Notebook |
| Right trigger | Interact, or use the equipped tool or weapon |
| Right grip | Inventory |
| Right menu | Examine |
| Right trackpad click | Pause menu |
| Left trackpad click | Recenter horizontal view |

#### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 (right stick click) | Drag or combine |
| Right menu | Go back; open an inventory item's actions |
| Right grip or Left menu | Close the current screen |

### Pico

The `pico4_controller` (Pico 4 and Pico 4 Ultra) and `pico_neo3_controller` (Pico Neo 3) profiles use the Meta Quest / Touch layout when connected through a SteamVR streaming bridge such as Pico Connect or Streaming Assistant.

## Left-handed controls

Set **Options → VR Settings → Handedness** to **Left** to mirror the controls between hands. Movement, turning, gameplay actions, inventory, and UI controls all change sides. The dominant hand remains the default for tools and pointing, while either bare hand can target, grab, throw, and receive interaction haptics; the notebook remains on the off hand.

Pause and recenter stay on the same physical controls in both modes. Changing handedness safely holsters an equipped tool and turns off a hand-held light before the controls switch.

The PS VR2 left-handed gameplay layout is:

| Control | Action |
| --- | --- |
| Right stick | Move |
| R3 | Sprint |
| Left stick | Snap or smooth turn |
| L3 | Toggle crouch |
| R1 | Toggle the current quick light |
| Cross (right) | Notebook |
| Circle (right) | Holster the equipped tool or weapon |
| Square (left) | Jump |
| L2 | Interact, or use the equipped tool or weapon |
| R2, while the off hand is bare | Interact with or grab a nearby object using the off hand |
| L1 | Inventory |
| Triangle (left) | Examine |
| Options (right) | Pause menu |
| Create (left) | Recenter horizontal view |

In menus, use either trigger to select, the left stick click to drag or combine, Triangle to go back, and L1, Cross, or Options to close the screen.

## Hand interaction and collision

Either bare hand can highlight and grab a nearby object. The hand that presses its trigger owns the grab, supplies release velocity, and receives pickup/drop feedback. If that hand has no target, interaction may fall back to a target already touching the other bare hand.

Every bundled profile ships a default binding for the off-hand trigger — L2/R2 on PS VR2 Sense and the index trigger on all other controller families — so off-hand grabbing works without manual mapping.

Moving an unoccupied hand into an ordinary dynamic prop or hinged door applies a bounded impulse at the contact point. Inventory pickups are excluded, and grabbable or movable props receive gentler limits so they remain easy to collect. This is a dynamic-object interaction proxy rather than a rigid hand collider: the rendered hand can still pass through static walls and scenery.

The original game exposes one active player-interaction state. Consequently, both hands can initiate interaction, but holding two different physics objects simultaneously or constraining one object with both hands is not yet supported.

## VR settings

Open **Options → VR Settings** to change controls, movement, calibration, and display options. Aim at a row and use R2 to increase or select the next value, R3 to decrease or select the previous value, and Circle to return.

Changes are saved immediately. Render scale requires a restart. Settings that do not apply to the selected turn or crouch mode are shown greyed out.

| Setting | Default | Valid range | Effect |
| --- | ---: | ---: | --- |
| `Handedness` | `Right` | `Left`, `Right` | Chooses the dominant hand and mirrors the controls when set to `Left` |
| `PlayMode` | `Standing` | `Standing`, `Seated` | Raises the world to the configured standing eye height in seated mode |
| `PlayerHeight` | `1.70` | `1.40`–`2.10` m | Configured standing height, set manually or with Calibrate |
| `MoveSpeed` | `1.0` | `0.25`–`3.0` | Multiplies smooth-locomotion speed |
| `MoveDeadZone` | `0.15` | `0.0`–`0.9` | Adjusts the movement-stick dead zone |
| `HeightOffset` | `0.0` | `-0.5`–`0.5` m | Adds a vertical calibration offset without changing world scale |
| `TurnMode` | `Snap` | `Disabled`, `Snap`, `Smooth` | Selects physical-only, stepped, or continuous turning |
| `SnapTurnAngle` | `45` | `15`–`90` degrees | Angle of each snap turn |
| `SmoothTurnSpeed` | `90` | `30`–`360` degrees/s | Maximum continuous turn speed |
| `TurnDeadZone` | `0.20` | `0.0`–`0.9` | Adjusts the turn-stick dead zone |
| `UIDistance` | `1.75` | `0.75`–`3.0` m | Distance of room-anchored menus, cinematics, messages, and subtitles |
| `UIScale` | `1.0` | `0.5`–`2.0` | Size of the room-anchored UI surface |
| `RenderScale` | `1.0` | `0.5`–`2.0` | Multiplies SteamVR's recommended eye-buffer size; requires a restart |
| `CrouchMode` | `Hybrid` | `Physical`, `Button`, `Hybrid` | Chooses body movement, the crouch button, or either |
| `PhysicalCrouchDepth` | `0.25` | `0.10`–`0.60` m | Sets physical crouch detection depth and the button-crouch view offset |
| `SubtitleScale` | `1.35` | `0.75`–`2.0` | Scales cinematic subtitles, radio text, and gameplay messages |

The Player Height row includes a **Calibrate** button, and the calibration section includes a one-press **Recenter** row. Snap turn requires the stick to return to the centre before another turn. Turning is disabled in menus and scripted look sequences.

Hybrid crouch is the default. Physical crouch follows headset movement; button crouch lowers the viewpoint and changes the real gameplay collider and stealth state. Seated mode raises the world to the configured player height without changing world scale. Standing is retried safely if a low ceiling blocks the full-height collider.

Menus and cinematics stay anchored in the room while the player moves their head. Recenter places the surface in front of the current view again. UI distance and UI scale can be adjusted independently.

The same screen offers Performance, Balanced, and Quality graphics presets, plus a desktop-mirror toggle. Any graphics combination that differs from those presets is labelled Custom.

## Haptic feedback

The bundled profiles provide feedback for UI selection, object pickup and drop, hand nudge, successful item use, melee impact, quick-light changes, and damage. Pickup, drop, and nudge feedback follows the interacting hand; most tool feedback is sent to the dominant hand, quick-light feedback to the off hand, and damage feedback to both hands.

## Hardware compatibility

PS VR2 Sense is the only controller family with a complete hardware pass. Other profiles are bundled but still need complete device-specific testing. Every bundled profile does ship default trigger bindings for off-hand interact, following each device's standard SteamVR inputs; those bindings are unvalidated on hardware outside PS VR2 Sense. See the [release history](RELEASES.md) for the dated validation record and current limitations.

| Profile | Coverage | Notes |
| --- | --- | --- |
| PS VR2 Sense | Hardware-tested | Experimental hands and equipped-object presentation still have known visual issues |
| HTC Vive | Bundled compatibility profile; not hardware-validated | Preserves the original Vive layout and leaves newer actions unassigned |
| Valve Index | Bundled default with early external feedback; full pass pending | Uses the `knuckles` profile |
| Meta Quest / Oculus Touch | Bundled default; not hardware-validated | Uses the `oculus_touch` profile; recenter is unassigned by default |
| Pico 4 / Neo 3 | Bundled default; not hardware-validated | Uses the Touch-style layout through SteamVR streaming bridges |
| Windows Mixed Reality | Bundled default; not hardware-validated | Supports both common SteamVR controller identifiers |

Visible hand animation remains experimental. Devices with skeletal input can animate individual fingers; other controllers use grip and trigger values. The current hand mesh limits independent ring, middle, and thumb movement.

## Troubleshooting input

After closing the game, open `Documents/Penumbra Overture/Episode1/hpl.log`:

- `SteamVR Input actions are active.` confirms that the action manifest and controller binding are in use.
- `Falling back to legacy controller polling.` means the compatibility input path is active.
- A `[VR input] ... matches bundled default profile ...` line confirms that the connected controller uses a shipped profile.
- A `WARNING [VR input]: ... has no bundled default profile ...` line means the controller is unknown to the mod. Controls may be incomplete; include `hpl.log` when reporting it.

If one controller loses tracking, its hand and pointer disappear and any held interaction is released. The other hand remains available for UI pointing.

### PS VR2 Bluetooth stability

PS VR2 Sense controllers connect to the PC independently over Bluetooth, so the headset can keep tracking while either hand disconnects. If SteamVR records `playstation_vr2: Send operation timed out. Closing device`, check the Bluetooth connection and driver before troubleshooting the game bindings.

- [PlayStation: Bluetooth adapters compatible with PS VR2 on PC](https://www.playstation.com/en-us/support/hardware/pc-ps-vr2-bluetooth/)
- [TP-Link: current UB500 PS VR2 support package](https://www.tp-link.com/en/support/faq/4188/)

After changing the driver, restart Windows, pair both Sense controllers again, and use **PlayStation VR2 App → Check Bluetooth Connection Quality** before another gameplay test. Sony also recommends a USB 2.0 port away from USB 3.0 devices or an extension cable with clear line of sight to the controllers.
