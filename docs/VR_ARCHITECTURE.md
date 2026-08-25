# VR architecture

The VR rework keeps runtime tracking, world placement, player state, and
gameplay interaction as separate concerns. World scale is always `1.0`.

This document describes the current `master` branch. Dated validation results
and known limitations belong in the [release history](RELEASES.md); upcoming
work belongs in the [roadmap](ROADMAP.md).

## Coordinate spaces

OpenVR poses enter the engine in **tracking space** and remain unchanged there.
The HMD and both controllers share the same tracking origin. Code that needs a
pose in Penumbra's world must use the single rigid **TrackingToWorld** transform
owned by `cVRTrackingSpace`.

The transform retains the historical mod's placement while adding an explicit
world yaw:

1. Keep the runtime HMD rotation.
2. Apply the configured world yaw around the current HMD anchor.
3. Anchor horizontal head position to the player's world position.
4. Apply the existing floor-reach height correction.
5. Derive one rigid TrackingToWorld matrix and apply it to HMD and hand poses.

Height calibration is stored separately from the rigid world transform. It
must never be implemented by changing world scale. The physical-crouch
standing baseline is calibrated in `cButtonHandler` on the gameplay side
(plausibility band 0.90–2.20 m, hysteresis); `cVRTrackingSpace` only receives
the resulting posture offset through `SetPostureOffset`.

The configured player height (VR Settings → Player Height, default 1.70 m,
range 1.40–2.10 m) is kept separate from the posture offset: it is a semantic
value chosen by the user or measured by the Calibrate button from the HMD
height. In standing play mode it is informational (recenter and posture offset
already handle the HMD anchor). In seated play mode `cButtonHandler` treats
real HMD height as the chair baseline: it captures the baseline while the HMD
sits in the plausible seated band (0.60–1.50 m), clears it when the player
stands up (HMD above baseline + 0.45 m), and derives `seatedOffset =
clamp(PlayerHeight − (baseline − 0.2 m) × 1.065, 0, 1.5 m)`, delivered to
`cVRTrackingSpace` through `SetSeatedOffset`. The seated offset raises the
world to the configured standing eye height without changing world scale, so
the player character and world objects behave as if the user were standing.

Create recenters horizontal orientation without teleporting the player
character. Snap and smooth turn change world yaw through the same relationship,
so the HMD, hands, held objects, physical head displacement, and world-space UI
remain coherent.

## Engine/game boundary

HPL owns generic runtime concerns: tracking poses, coordinate conversion,
input actions, and haptic output. Penumbra owns gameplay meaning: which entity
a hand targets, what it holds, interaction rules, locomotion, crouch state, and
UI behaviour.

This avoids a monolithic VR subsystem that would make engine code depend on
Penumbra entity types.

## UI and loading

Fullscreen menus, cinematics, the death screen, and message surfaces capture a
room-space anchor when opened. HMD tracking remains live while the surface
stays stable; recentering rebuilds the anchor. `UIDistance`, `UIScale`, and
`SubtitleScale` change presentation without altering tracking or world scale.

Blocking transitions use a staged path: submit a black loading frame, keep the
SteamVR compositor fade opaque during the synchronous load, and reveal the new
state only after it has presented a frame. Map changes, the tutorial, new game,
continue, and explicit save loading share this path.

## Hands and equipped objects

`cSteamVRInput` exposes logical left/right poses plus a skeletal hand summary.
`PlayerHands.cpp` turns per-finger curl values into smoothed, rotation-only rig
poses; devices without skeletal data use a grip/trigger closing sequence.
Tracking input remains an engine concern, while rig geometry and gameplay
meaning stay in Penumbra.

Target and held-body state is recorded independently for both hands. The
configured dominant hand remains the default for tools and pointer origin, but
either bare hand can select and initiate a world interaction. Grab ownership,
throw velocity, hand visibility, and pickup/drop haptics follow the controller
that pressed interact. Invalid poses hide and release only the affected hand.

Unoccupied hands use a small overlap proxy to nudge dynamic props and hinged
doors with bounded contact-point impulses. Inventory items are excluded and
grabbable props use gentler limits. This does not create rigid collision against
static scenery. The original global player-state machine also limits the current
implementation to one active grab; simultaneous and two-point holds remain
future work.

Equipped lights, melee weapons, and thrown objects are separate attachments on
top of the visible hand rig. They share the controller pose, apply a fixed palm
socket plus per-item transform, and retain their original gameplay logic. This
presentation layer is experimental; current visual limitations are listed in
the [roadmap](ROADMAP.md) and [release history](RELEASES.md).

## Runtime and build constraints

- Rendering uses the established OpenVR compositor path; SteamVR Input actions
  have a legacy OpenVR polling fallback. Native OpenXR is not implemented.
- The executable remains Win32 because Newton, FLTK, SDL, OpenAL, Cg, and other
  checked-in dependencies are 32-bit. It is marked Large Address Aware for more
  memory headroom on 64-bit Windows.
- Release builds perform a full solution `Rebuild`. Legacy project metadata can
  otherwise reuse objects compiled against old class layouts and corrupt the
  heap. `/FS` serializes parallel PDB writes.
- World scale remains `1.0`; calibration, seated play, crouch, turning, and
  recentering operate through explicit offsets or the TrackingToWorld boundary.
