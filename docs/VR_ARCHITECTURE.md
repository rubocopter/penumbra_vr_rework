# VR architecture

The VR rework keeps runtime tracking, world placement, player state, and
gameplay interaction as separate concerns. World scale is always `1.0`.

Current state: this documents the `development` branch. The completion notes
below describe when each increment was implemented and validated on PS VR2
hardware; the crouch and hand-rig areas were changed after their hardware
validation (16–18 August 2026) and are the parts most likely to differ from
those notes. The animated hands are experimental and frozen for now.

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

## Planned increments

1. Central TrackingToWorld boundary without a gameplay change. **Complete.**
2. Independent left/right hand target and held-object state; reuse interaction
   collision geometry. **Complete as infrastructure; only right-hand gameplay
   interaction is intentionally enabled.**
3. Persistent declarative VR settings and an in-game editor under
   `Options → VR Settings`. **Complete; editing and persistence validated on
   PS VR2 hardware.**
4. Snap/smooth/disabled turn, recenter, movement deadzone, and speed.
   **Complete; validated on PS VR2 hardware.**
5. Physical, button, and hybrid crouch with a standing-height baseline and
   configurable displacement threshold. Button crouch lowers both the gameplay
   collider and rendered viewpoint without changing world scale. **All three
   modes were validated on PS VR2 hardware on 14 August 2026. Since then
   (16–18 August 2026) button crouch lowers the viewpoint through a posture
   offset in the TrackingToWorld transform by the configured crouch depth
   (0.25 m default) instead of the deeper original move-state offset, and
   physical crouch calibrates the standing HMD height during gameplay with a
   plausibility band (0.90–2.20 m) and hysteresis. These changes have not had a
   full hardware retest; low-ceiling recovery is still pending.**
6. Configurable main-menu/cinematic UI distance and scale. Fullscreen surfaces
   capture a room-space anchor when opened; HMD tracking remains live and
   recentering rebuilds that anchor. **Complete; placement and recentering
   validated on PS VR2 hardware.**
7. Centralized haptic profiles, per-event repeat limits, and gameplay feedback.
   **Implemented; initial damage and melee behaviour validated on PS VR2, with
   further per-event tuning still open.**
8. Staged synchronous loading: submit a black loading frame, keep an opaque
   compositor fade during the blocking load, then reveal the new map. Map
   changes, initial/new-game maps, the VR tutorial, and save restoration share
   this path. **All listed routes validated on PS VR2 hardware.**
9. Inventory ray/reticle and restored action-popup text.
   **Implemented, generalized to all interactive UI surfaces, and able to fall
   back to the left hand if the right pose is unavailable. Pointer, fallback,
   and right-hand pose reacquisition validated on PS VR2 hardware.**
10. Room-scale collision reconciliation: when HPL rejects physical head-driven
    body motion, subtract the rejected component from the VR tracking origin.
    VR move interactions retain HPL1's native character collision, preventing a
    held swing door from becoming non-solid to the player. **Validated against
    the held locked-door bypass and ordinary collision on PS VR2 hardware.**
11. Cinematic presentation uses a room-anchored surface and scalable text.
    Opaque letterbox and subtitle bands are omitted so they cannot cover the
    image; intro-image render state is restored independently, so toggling
    subtitles cannot suppress the film. **Image rendering was validated with
    subtitles enabled and disabled; the final no-band presentation extends the
    already verified disabled-subtitle path to both states.**
12. Keep the executable Win32 for legacy binary compatibility while marking it
    Large Address Aware, allowing a 64-bit Windows host to provide the process
    with a larger user-mode address space. **Implemented and verified in the
    Release PE header.**
13. Player physics handles have two explicit lifetime paths. Normal map changes
    destroy their individual objects while the outgoing world is valid; a full
    updater reset only nulls the handles and leaves complete-world destruction
    to MapHandler. This avoids deleting the same HPL1/Newton ownership graph at
    two different levels. **Revised after dump analysis found heap corruption
    during new-game reset; tutorial, new, continue, and save routes passed the
    clean hardware retest.**
14. Release builds use a full solution rebuild. The legacy Visual Studio
    dependency metadata did not invalidate `Init.obj` after class-layout
    changes in `SaveHandler.h` and `MapHandler.h`; mixing its old allocation
    sizes with their new constructors caused startup heap corruption. MSBuild
    also receives `/FS` so parallel compiler processes serialize PDB writes.
    **Build pipeline corrected and its clean Release output validated across all
    startup/load routes on hardware.**
