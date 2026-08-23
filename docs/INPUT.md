# SteamVR input

Penumbra VR uses SteamVR Input actions instead of assuming fixed HTC Vive button IDs and axis numbers. Gameplay and UI have separate action sets, so a physical control can have an appropriate meaning in menus without leaking into movement or interaction. If the action manifest cannot be initialized or no actions are active, the game automatically falls back to the historical OpenVR controller polling path.

Both paths are normalized into the same intent-named runtime state. Gameplay
therefore consumes actions such as interact, inventory, sprint, or UI select
without depending on Vive-era button field names.

The action manifest is installed as `vr/actions.json` beside `Penumbra_vr.exe`. Bindings can be inspected and customized through SteamVR's controller binding interface.

## Device-independent control scheme

Gameplay consumes semantic actions rather than PS VR2 or Vive button names.
Each SteamVR controller profile maps its own physical controls to these intents:

| Intent | Gameplay meaning |
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
| UI alternate | Decrease a VR setting or use the original inventory drag/combine action |

The physical labels below describe the bundled profiles, not universal prompts.
Custom profiles can remap these actions in SteamVR without changing gameplay
code.

## PS VR2 Sense defaults

### Gameplay

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
| R1 | Inventory |
| Circle | Examine |
| Options | Pause menu |
| Create | Recenter horizontal view |

### Menus and inventory

| Control | Action |
| --- | --- |
| Aim with the dominant-hand controller; the other hand if that pose is unavailable | Move the pointer |
| R2, or the off-hand trigger during fallback | Select; in VR settings, increase or choose the next value; in inventory, perform the default action |
| R3 | In VR settings, decrease or choose the previous value; in inventory, drag or combine items |
| Circle | Go back in menus; in inventory, open an item's actions or go back |
| R1, Square, or Options | Close the current screen |

When a tool such as the hammer is equipped, R2 operates that tool. Press Triangle to holster it before using R2 to interact with doors and other objects again.

To use an inventory item on the world, open the inventory with R1, aim at the
item with the dominant-hand controller, and press R2. The inventory closes with
the item readied; aim at its target from interaction range and press R2 again.
R2 also selects an action opened with Circle, while R3 remains available for
the original drag-and-combine interaction.

Input edges are latched when the game switches between gameplay and UI action
sets. A button that opened a screen must be released and pressed again before
its UI binding can close that screen.

The main menu, inventory, notebook, and numerical panels use the same tracked
ray, cyan endpoint reticle, and lightly smoothed cursor. The configured
dominant hand is the primary pointer; if its SteamVR pose is unavailable, the
other hand takes over and its trigger can select. The original mouse path
remains available. Item action popups now render their localized action names
instead of the empty highlighted box left by the historical mod.

These tables document the current bundled binding profiles (PS VR2 Sense, HTC
Vive, Valve Index, Meta Quest/Oculus Touch, Windows Mixed Reality); they are
not intended as universal in-game button names. Player-facing control prompts
will remain unchanged until the game can resolve labels from the active
SteamVR controller profile instead of hard-coding one headset's layout.

## Left-handed mirror

With `Handedness = Left`, the game activates the fully mirrored action sets
`/actions/gameplay_left` and `/actions/ui_left` declared in `vr/actions.json`
and bound in every bundled profile. The intent is the same as the default
layout (movement, sprint, notebook, holster, UI select, and so on), but every
control is the left/right mirror of the tables above: what the default layout
put on the right hand now sits on the left hand and vice versa. Equipment
slots follow the same rule in gameplay code, so the dominant hand stays free
for the pointer, interaction, and tools while the off hand carries the
inventory and quick light.

For example, the PS VR2 left-handed gameplay layout is:

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
| L1 | Inventory |
| Triangle (left) | Examine |
| Options (right) | Pause menu |
| Create (left) | Recenter horizontal view |

The PS VR2 driver exposes only two face buttons per hand (left: square and
triangle; right: cross and circle), so the mirrored layout moves every action
to the same physical button position on the other hand: jump is the bottom
button of the dominant hand, notebook the bottom button of the off hand,
examine the top button of the dominant hand, and holster the top button of
the off hand. The UI layout mirrors the same way: select on L2 or R2, drag on
the left stick click, back on Triangle (left), close on L1, Cross (right) or
Options, and recenter on Create (left). Pause and recenter stay on the same
physical controls as the default layout (Options and left Create), because
binding the left Create to two conflicting actions proved unreliable in
SteamVR. The global set (poses, haptics, skeletons) is not mirrored because
those actions are physical and hand-agnostic. Interact target detection also
accepts the left hand's rig name (`LeftHand`), so the mirrored layout can use
tools and grab objects with the dominant left hand. Only the dominant hand
performs grab-target detection and shows the grab icon at grab distance; the
off hand can never grab, so it stays bare and never highlights objects.
Changing the handedness in VR Settings returns the player to bare hands
(tools holstered, lights off) before the mirror takes over, and latches input
edges for one frame so a held button cannot fire on the action it was swapped
onto; the legacy OpenVR fallback mirrors the same layout by reading the
dominant and off hands. The notebook always hangs on the off hand (the hand
not used for pointing), so the dominant hand keeps the pointer free. The
notebook is an 800x600 px UI surface anchored to the off hand; the visible
book (350x460 px) is placed with the hand at its outer edge and extends
about a book width (0.24 m) toward the body centre in both dominant modes
(the left off hand grips the book's left edge, the right off hand its right
edge, because the engine mirrors the controller pose). Opening it returns
both hands to bare exactly like a handedness change, since it is a
navigation-only view. The mirror was added on 19 August 2026 and awaits a
final hardware regression pass. Recenter is the only action exempted from the
context/latency latch beyond a dominant-hand change, and it counts as an
active action on every frame: pressing Create while standing still is often
the only input that frame, and without that exemption an idle frame could
fall back to legacy polling and drop the press (recenter worked in one
dominant mode but not the other, depending on which actions happened to be
active). The PS VR2 legacy binding has no Create source, so the fallback path
cannot recover it; the fix on 20 August 2026 keeps the press on the action
path.

## Hardware validation status

This table records what has been exercised on real hardware and when. It
describes those milestone builds: the crouch and hand areas changed after the
dates below (16–18 August 2026). The crouch and height changes have since been
revalidated and are confirmed correct; the hand fold state is frozen as an
experimental final and should be treated as such until it is reworked.

| Profile | Status | Validated behaviour |
| --- | --- | --- |
| PS VR2 Sense | Hardware-tested, 14–15 August 2026 | Gameplay/UI actions, repeated inventory transitions, all crouch modes, snap/smooth turn, recenter, pointer, tutorial/new/continue/load routes, pose loss and reconnection, rigged hands (15 August) |
| HTC Vive | Compatibility profile retained | Historical layout is represented in SteamVR Input but this rework has not yet been retested on Vive hardware |
| Valve Index | Bundled default profile, not yet hardware-validated | `knuckles` profile follows the PS VR2 scheme; recenter and pause stay on the left trackpad and left system button in both handedness modes |
| Meta Quest / Oculus Touch | Bundled default profile, not yet hardware-validated | `oculus_touch` profile follows the PS VR2 scheme; recenter is left unassigned for SteamVR mapping |
| Pico 4 / Neo 3 | Bundled default profile, not yet hardware-validated | `pico4_controller` and `pico_neo3_controller` mirror the Touch layout for SteamVR streaming bridges |
| Windows Mixed Reality | Bundled default profile, not yet hardware-validated | `microsoft/motion_controller` and `holographic_controller` profiles keep the WMR trackpads for pause and recenter |

Every bundled profile must cover the mandatory actions (move, interact, and UI
select) or the project validation fails. Custom bindings created in SteamVR
replace these defaults per controller type without changing gameplay code.

## Persistent VR settings

Implemented VR comfort values are stored in the normal user `settings.cfg`
under a dedicated `[VR]` section. On Windows this file is normally at
`Documents/Penumbra Overture/Episode1/settings.cfg`.

They can be changed in-game under **Options → VR Settings**. The menu is
grouped into four sections — CONTROLS (handedness), MOVEMENT (locomotion and
turning), CALIBRATION (play mode, height, recenter), and DISPLAY (UI distance
and scale). Aim at a setting with the dominant-hand controller, press R2 to
increase/select the next value, or R3 to decrease/select the previous value.
Changes apply immediately and are saved at once. Circle returns to the previous
menu without also changing the highlighted value. The player-height row has a
Calibrate button on the same line, and the CALIBRATION section ends with a
Recenter row that re-orients the playspace in one press. Settings that do not
apply to the current configuration are shown greyed out: with Turn Mode on
Smooth the snap angle is disabled, with Turn Mode on Snap the smooth speed is
disabled, and with Turn Mode on Disabled both are disabled. With Crouch Mode on
Physical the depth row is relabelled "Detection depth" (the headset-descent
threshold) instead of the viewpoint-lowering depth.

| Setting | Default | Valid range | Effect |
| --- | ---: | ---: | --- |
| `Handedness` | `Right` | `Left`, `Right` | Chooses the dominant hand driving interaction, pointer, throw, and haptics; `Left` also activates the fully mirrored action set |
| `PlayMode` | `Standing` | `Standing`, `Seated` | Seated mode raises the world to the configured standing eye height |
| `PlayerHeight` | `1.70` | `1.40`–`2.10` m | Configured standing height, set manually or with Calibrate |
| `MoveSpeed` | `1.0` | `0.25`–`3.0` | Multiplies smooth-locomotion speed |
| `MoveDeadZone` | `0.15` | `0.0`–`0.9` | Radial dead zone, rescaled outside the centre |
| `HeightOffset` | `0.0` | `-0.5`–`0.5` m | Adds a vertical calibration offset without changing world scale |
| `TurnMode` | `Snap` | `Disabled`, `Snap`, `Smooth` | Selects physical-only, stepped, or continuous turning |
| `SnapTurnAngle` | `45` | `15`–`90` degrees | Angle applied for each snap turn |
| `SmoothTurnSpeed` | `90` | `30`–`360` degrees/s | Maximum continuous turn speed |
| `TurnDeadZone` | `0.20` | `0.0`–`0.9` | Horizontal right-stick dead zone |
| `UIDistance` | `1.75` | `0.75`–`3.0` m | Distance of room-anchored main menus, cinematics, the death screen, examine/gameplay messages, radio subtitles, and cinematic subtitles |
| `UIScale` | `1.0` | `0.5`–`2.0` | Size multiplier for that frontal UI surface |
| `CrouchMode` | `Hybrid` | `Physical`, `Button`, `Hybrid` | Chooses body movement, the bound crouch action, or either |
| `PhysicalCrouchDepth` | `0.25` | `0.10`–`0.60` m | Headset descent below the standing baseline that enters physical crouch; also the view offset applied by button crouch |
| `SubtitleScale` | `1.35` | `0.75`–`2.0` | Scales cinematic subtitles, radio text, and gameplay messages |

Out-of-range values are clamped and the effective values are written to the
game log on startup. Snap turn requires the stick to return through its dead
zone before another step can fire. Turning is disabled in menus and scripted
look sequences; recenter remains bound to the dominant Create (or equivalent
per-hand control) in each action set. UI distance and scale are independent so the
surface can be moved farther away without forcing it to occupy less of the
view.

The world-interaction ray uses the SteamVR `aim` pose (`/user/hand/*/pose/aim`,
bound as `left_aim`/`right_aim` in every bundled profile) instead of the grip
pose, so pointing matches where the controller naturally aims on each device.
Drivers or bindings that do not expose an aim pose fall back to the grip pose
automatically; a missing aim binding never invalidates hand tracking.

Hybrid crouch is the default. The physical path calibrates a standing HMD
baseline during gameplay, rejects samples outside the plausible height band
(0.90–2.20 m, so a displaced tracking-space sample cannot become the baseline
after loading a save), and uses hysteresis to avoid flicker at the threshold;
the button path latches the semantic crouch action. Both change Penumbra's real
gameplay collider and stealth/movement state. Button crouch also lowers the
rendered viewpoint by the configured crouch depth (0.25 m by default) through a
posture offset in the TrackingToWorld transform, while physical crouch keeps
the user's real head displacement. Neither changes world scale, and standing is
retried safely if a low ceiling blocks the full-height collider. The crouch
and height changes of 16–18 August 2026 have been revalidated on hardware and
are confirmed correct.

Seated play mode (VR Settings → Play Mode) keeps the same chain without
touching world scale. While the HMD height sits in the plausible seated band
(0.60–1.50 m), `cButtonHandler` treats it as the chair baseline; standing up
above the baseline by more than 0.45 m clears it. The seated offset is the
configured player height minus the baseline eye height
(`clamp(PlayerHeight − (baseline − 0.2 m) × 1.065, 0, 1.5 m)`) and is applied
at the TrackingToWorld transform, so the player character and world objects
behave as if the user were standing. The Player Height row's Calibrate button
measures the HMD height while the user stands straight and writes it back to
the setting (samples outside 0.90–2.20 m are rejected). In standing mode the
configured height is informational.

Fullscreen menus and cinematics are captured at a stable room-space pose when
opened rather than following every head movement. Recenter places the surface
in front of the current view again. Synchronous map changes, initial/new-game
maps, the VR tutorial, and saved-game loads first present a black `Loading...`
frame and hold both compositor fade layers until a frame from the loaded state
has actually been presented. All of these routes were exercised on PS VR2
hardware. Cinematic images and scalable subtitles are rendered without opaque
letterbox/subtitle bands so the image is not unnecessarily covered in the
headset.

## Haptic feedback

The SteamVR action path routes semantic feedback to the logical controller
hands. The initial profiles provide a subtle dominant-hand pulse for UI
selection, object pickup/drop, successful item use, and melee impact;
quick-light feedback is sent to the off hand; damage is sent to both hands.
Per-event repeat limits filter transient contact jitter without reducing the
strength of a valid pulse. The game log records each submitted event as
`[VR haptics +... ms] ... (submitted)` so controller-side testing can distinguish a
gameplay event that was generated from one that never reached SteamVR.

## Hand pose data (grip / trigger)

The action manifest exposes two skeleton actions, `/actions/global/in/left_skeleton`
and `/actions/global/in/right_skeleton`, declared against `/skeleton/hand/{left,right}`
and bound to `/user/hand/{left,right}/input/skeleton/{left,right}` in the PS VR2 Sense,
Valve Index, and Touch profiles. Every frame the engine asks
SteamVR for the *device* skeletal summary and turns the per-finger curl values
(0 = straight, 1 = fully curled) into one normalized pair per hand:

| Value | Source |
| --- | --- |
| `trigger` | Curl of the index finger |
| `grip` | Strongest curl among middle, ring, and pinky |

The summary lives on the tracked-hand state beside the pose (validity,
velocity, and button data), so gameplay code never touches SteamVR directly.
It also retains the raw per-finger curls (plus a `skeletal` flag) so hand
rendering can drive every finger individually on devices that measure them.

Start the game and check `hpl.log` for lines like
`[VR hand +...ms] L grip 0.00 trigger 0.00 (skeleton).`, which are printed for
each hand every 250 ms while a summary is valid. The expected PS VR2 Sense
behaviour is:

1. Open hand: grip ≈ 0, trigger ≈ 0.
2. Squeeze the grip: grip rises while trigger stays ≈ 0.
3. Pull R2: trigger rises.
4. Release: both return to 0, identically on the left and right hand.

Controllers without skeletal support (HTC Vive wands, and Windows Mixed
Reality when it falls back to polling) have no skeleton binding, so their
summary stays invalid and the hands keep their previous open pose. On the
legacy polling path the summary is instead derived from the button state:
grip becomes binary (0/1) while the trigger keeps its analog margin.

The game-side hand animation builds one rotation-only pose track per finger
chain at rig setup (`PlayerHands.cpp`: HandMiddle, HandRing, HandLittle,
HandIndex, HandThumb, one keyframe per bone with the tuned angles and axes)
and each frame drives every finger's blend weight independently:

- **Skeletal controllers** (PS VR2 Sense, Index, Touch): each finger follows
  its own measured curl from the device summary, so individual fingers can
  move separately.
- **Legacy devices** (Vive wands, WMR polling fallback): only a grip/trigger
  pair exists, so the closing sequence is synthesized with staggered windows —
  pinky starts first, then ring, then middle, and the thumb opposes last;
  the index stays tied to the trigger. Each window applies an ease-in-out
  curve so fingers accelerate mid-curl instead of folding linearly.

All weights pass through an exponential smoother (~70 ms time constant) that
removes sensor jitter and gives the joints slight inertia. The hands remain
experimental: after hardware feedback on 21 August 2026 the fold directions
were corrected with `scripts/verify_fold_directions.py` — the left rig is a
Y-mirror of the right, so every fold axis flips its X/Z sign per hand; the
four fingers share one plain ±Z axis (curling around diverging anatomical
axes stretched the fused Ring/Middle mesh tube into a visible double finger),
and the thumb uses a YZ-tilted per-hand axis that sweeps it toward the index
zone. The grab wrap reaches 170 deg total per finger. The thumb also always
follows the grip: many devices report a constant ~0 thumb curl, which would
otherwise leave it frozen while the rest of the hand closes. Known
limitations: the ring and middle fingers share a fused mesh tube, the index
has a limited free tube, and the thumb chain is too short to reach the palm
flesh. The `[hand-rig]` log lines report the bone indices, axes, angles, and
engine rotations at rig setup, plus grip/trigger values and per-finger
weights after every change, which helps diagnose fold problems.

## HTC Vive compatibility defaults

The Vive binding preserves the historical layout:

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

Turn, button crouch, holster, pause, and recenter were introduced after the
original Vive layout and are not assigned in the bundled compatibility profile.
They can be mapped through SteamVR, but a revised Vive default should be tested
on real hardware before distribution.

## Valve Index defaults

The bundled `knuckles` profile follows the PS VR2 scheme, adapted to the Index
controllers. The left trackpad click recenters the view and the left system
button opens the pause menu in both handedness modes.

### Gameplay

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

### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 | Drag or combine |
| Right B | Go back; in inventory, open an item's actions |
| Right grip or Left B | Close the current screen |

## Meta Quest and Oculus Touch defaults

The bundled `oculus_touch` profile covers Quest 2/3/Pro and Rift controllers
connected through SteamVR Link. There is no spare Create-style button on Touch,
so the recenter action is left unassigned; it can be mapped through SteamVR.

### Gameplay

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

### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 | Drag or combine |
| Right B | Go back; in inventory, open an item's actions |
| Right grip or Left X | Close the current screen |

## Windows Mixed Reality defaults

The bundled `microsoft/motion_controller` and `holographic_controller`
profiles are identical: SteamVR drivers report WMR controllers under either
identifier depending on version, so both entries resolve to the same control
scheme. The profiles use the WMR joysticks as the
primary movement inputs and keep the trackpads for secondary functions. The
left trackpad click recenters the view and the right trackpad click pauses in
both handedness modes.

### Gameplay

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

### Menus and inventory

| Control | Action |
| --- | --- |
| Left or right trigger | Select |
| R3 (right stick click) | Drag or combine |
| Right menu | Go back; in inventory, open an item's actions |
| Right grip or Left menu | Close the current screen |

## Pico defaults

The `pico4_controller` (Pico 4, Pico 4 Ultra) and `pico_neo3_controller`
(Pico Neo 3) profiles mirror the Touch layout because Pico controllers
connected to SteamVR expose the same joystick, trigger, and grip controls.
They are reached through SteamVR streaming bridges such as Pico Connect or
Streaming Assistant.

## Runtime diagnostics

On Windows, inspect `Documents/Penumbra Overture/Episode1/hpl.log` after closing the game. It reports one of these paths:

- `SteamVR Input actions are active.` means the action manifest and controller binding are in use.
- `Falling back to legacy controller polling.` means SteamVR Input reported every action origin inactive for over half a second (SteamVR occasionally refreshes bindings this way) and the historical input path is being used. Single-frame dropouts are bridged with the last known state instead of dropping input.

At startup (and briefly after, while SteamVR finishes enumerating devices) the log also records the detected hardware profile:

- `[VR input] left hand controller type 'knuckles' matches bundled default profile 'bindings/knuckles.json'.` confirms that the connected controller is covered by one of the shipped default bindings.
- `WARNING [VR input]: ... has no bundled default profile in actions.json.` means the controller model is unknown to the mod: SteamVR applies its generic binding, unmatched actions drop to legacy polling, and controls may feel partial. If you see this line, please report it along with your `hpl.log` — new default profiles are data-only additions.
- `[VR input] legacy fallback movement` note: when the fallback path is active, movement reads the strongest qualifying analog axis, so joysticks without touch events (Index, WMR thumbstick on axis 1) also drive movement.

Controller poses use the logical left/right SteamVR pose actions, with the
existing OpenVR tracked-device role loop retained as a compatibility fallback.
The log reports when each action pose is acquired or lost. While a pose is
invalid, the stale hand and its UI ray are hidden, spatial selection is blocked,
and a held interaction is released cleanly. UI pointing falls back to the left
hand if only the right pose is lost; non-spatial controls such as closing a
screen remain available.

## PS VR2 Bluetooth stability

PS VR2 Sense controllers communicate with the PC independently over Bluetooth.
A headset can therefore keep tracking normally while both hands disconnect. If
SteamVR records `playstation_vr2: Send operation timed out. Closing device`, the
failure is below the game input layer.

Sony lists specific adapter and placement guidance. In particular, the TP-Link
UB500/UB5A has a documented PS VR2 compatibility issue and requires TP-Link's
PS VR2 stability driver rather than relying on an unrelated Windows-supplied
package. TP-Link's global FAQ, updated on 30 September 2024, links
`UB500_BT_1.9.1038.3020.zip`. Its signed INF includes the UB500 hardware ID
`USB\VID_2357&PID_0604` and supersedes the `1.9.1038.3001` package still linked
by the older Spanish localization of the same FAQ:

- [PlayStation: Bluetooth adapters compatible with PS VR2 on PC](https://www.playstation.com/en-us/support/hardware/pc-ps-vr2-bluetooth/)
- [TP-Link global: latest UB500/UB5A PS VR2 stability driver](https://www.tp-link.com/en/support/faq/4188/)
- [TP-Link Spain: older localized version of the same FAQ](https://www.tp-link.com/es/support/faq/4188/)

After changing the driver, restart Windows, pair both Sense controllers again,
and use **PlayStation VR2 App → Check Bluetooth Connection Quality** before a
gameplay test. Sony also recommends a USB 2.0 port away from USB 3.0 devices or
an extension cable with clear line of sight to the controllers.
