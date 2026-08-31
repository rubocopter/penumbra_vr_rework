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

Requested crouch (physical, button, or hybrid) is kept separate from the
effective stance. When the full-height character collider cannot leave crouch,
the environment temporarily constrains the effective stance and applies the
same safe virtual height drop as button crouch. The original request is not
changed. Full-height standing is retried against world collision each frame and
returns automatically once there is clearance and no crouch request remains.
The VR crouch controller consumes both SteamVR and keyboard edges and is the sole
owner of the collider transition; legacy state callbacks do not toggle the same
edge again. Floor-only upward contact is ignored during the standing probe, while
downward ceiling or lateral correction still blocks recovery.

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
that pressed interact. Invalid poses have a short grace period and hide only the
affected hand, preventing one controller dropout from removing both rigs.

The raw controller pose remains the source of intent. Before rendering a bare
hand, `cPlayer` validates the complete rigid transform and resolves one cached
pose for every consumer of that OpenVR sample. The physical palm is a
`190 × 52 × 125 mm` box. Translation is sampled every 20 mm, refined at the
first blocking contact, and allowed to slide only by removing motion into the
selected surface normal. Rotation is swept separately so a wrist turn cannot
expand the long palm axis through a surface. Interaction queries use a
scale-free physical palm transform instead of the visual mesh scale, so reaching
and contact distances remain in metres. This is deliberately a coarse
palm/hand volume, not per-finger rigid-body physics; thin geometry and individual
fingers can still clip.

Normal world contact accepts a 2 mm numerical skin. When the hand is pressing
toward a nearby non-magnetic physical target, the skin may increase to 8 mm to
reduce the stand-off at drawer fronts and handles. The target body is still
tested and the full palm dimensions remain unchanged. A 20 mm sweep step is
smaller than the palm's thinnest blocking span even with that larger skin, so a
fast controller cannot skip a thin wall or closed door between samples.

The first valid pose after a load is accepted directly when collision-free;
there is no artificial shoulder-to-controller trajectory. If it begins inside a
ceiling or moving mechanism, or if a hand stays more than 10 cm behind its
controller in a concave contact and the player starts pulling back, `cPlayer`
searches several collision-free anchors beside and below the head. Recovery then
uses the same full-palm sweep from the selected anchor to the controller. Pushing
farther into the obstruction never activates recovery, and an anchor on the
player side cannot cross an intervening wall or closed door. Large tracking
discontinuities use the same guarded path.

Unoccupied hands use the raw controller path and a small overlap proxy to nudge
dynamic props and constrained mechanisms. The response adapts to mass, size,
breakability, and the active hinge or slider: small inventory items receive more
help, while barrels, loaded crates, and other heavy or breakable bodies are kept
gentle. The visible hand may stop at scenery while this bounded assist still lets
the player retrieve an item from a cramped shelf. The original global
player-state machine limits the current implementation to one active grab;
simultaneous and two-point holds remain future work.

When that bounded raw-controller extension is used, the contact target must be
the first solid body reached from the collision-resolved palm. The very-short
contact shortcut is retained only when no meaningful extension was needed. This
prevents a thin closed door from being mistaken for direct palm contact while
preserving forgiving acquisition of visible drawer handles.

Normal palm overlap remains the first interaction target. If it finds nothing,
each bare hand also performs an aim-cone query for inventory items only. The
query ranks candidates by angle, distance, and item type, samples the visible
portion of their bounding box, and requires unobstructed solid-body rays from
both the controller and the tracked head. The head ray is essential when the
physical controller has crossed a closed door even though the rendered hand is
collision-constrained. Only the five best geometric candidates are ray-tested,
and the broad phase is a tight segment AABB rather than a hand-centred cube.
Consumables and stacks receive the longest reach, ordinary items an intermediate
one, and important equipment a shorter one. This permits direct acquisition of
batteries, healing items, keys, and similar pickups inside narrow shelves
without adding magnetic forces to props, drawers, doors, or mechanisms.
Candidate lock uses a subtle per-hand haptic pulse and no additional HUD marker.
Candidate lock also renders a thin blue line from the visible item sample to the
posed middle-finger knuckle. It is drawn directly by the normal world player state,
not through the inventory/menu pointer, and is suppressed whenever those UI
surfaces are active.

Equipped lights, melee weapons, and thrown objects are separate attachments on
top of the visible hand rig and retain their original gameplay logic. Each HUD
definition can provide `VrGripPoint`, `VrGripRadius`, and `VrGripTwist`. The grip
point is aligned with a cylinder fitted through the closing fingers, radius
drives the amount of finger curl, and twist rotates the item around that contact
axis. `scripts/analyze-vr-grips.py` reconstructs HUD geometry and hand skeleton
poses so bundled tools can be calibrated against measured bounds rather than by
screen-space guesswork. Rig limitations, especially the short static thumb chain
and fused ring/middle mesh, still constrain the final pose.

## Rendering and lighting

Each eye renders to an `RGBA8` colour texture with a packed
`GL_DEPTH24_STENCIL8` renderbuffer. `RenderZ()` writes both depth and the
texture-modulated ambient base; `RenderLight()` subsequently adds direct lights
and retains the original stencil shadow-volume implementation. The desktop
monitor is a distinct render boundary and is marked non-VR by the low-level
graphics state.

There is currently no SSAO, sampleable depth attachment, HDR path, or
hemispherical ambient shader. A minimal SSAO feasibility prototype was measured
and then completely removed because its subtle contact darkening did not justify
its stereo cost and complexity. The verified current pipeline, measurements,
decision record, and tightly scoped design for a possible VR-only
hemispherical-ambient A/B are documented in [Rendering and lighting
research](LIGHTING.md).

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
