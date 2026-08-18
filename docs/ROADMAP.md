# Modernization roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

The rework is developed on the `development` branch. The hardware validation
dates below describe when each milestone was exercised on PS VR2; the code has
changed after some of them (notably the crouch and hand work of 16–18 August
2026), so a dated validation describes the milestone build, not a guarantee
about the current one. The crouch and height changes have since been
revalidated and are confirmed correct; the hand fold state is frozen as an
experimental final and currently shows incorrect visual behaviours.

## Phase 0 — reproducible baseline

- [x] Build all three native projects with Visual Studio 2022.
- [x] Remove author-specific paths and keep generated files outside source directories.
- [x] Produce a deterministic package layout with checksums.
- [x] Add reversible, manifest-owned synchronization into a Steam installation.
- [x] Build the same target in GitHub Actions.
- [x] Confirm that the historical executable starts from a separate Steam-derived installation.
- [x] Confirm baseline SteamVR operation with PS VR2 on PC.
- [x] Add and validate Spanish localization for the VR tutorial, menu, and controller notes.

Baseline validation began on 13 August 2026. The Release build launched through
SteamVR on PS VR2, retained the historical mod's behaviour, and loaded the VR
tutorial in Spanish. Deployment now targets the normal Steam installation so
Steam launch and play-time tracking continue to work; originals and all
mod-owned files are managed by a reversible manifest under `.penumbravr`.
Missing Spanish menu keys in the legacy translation had previously resolved to
empty strings, hiding the VR tutorial option; the English and Spanish key sets
are now kept in sync by project validation.

## Phase 1 — runtime and input boundary

- [x] Route HMD and controller world poses through one explicit TrackingToWorld transform.
- [x] Update the bundled OpenVR SDK and Win32 loader from 1.0.2 to 2.15.6.
- [x] Introduce device-independent gameplay and UI action sets with an automatic legacy fallback.
- [x] Add explicit default bindings for PS VR2 Sense and HTC Vive controllers.
- [x] Expose device-independent pose and haptic actions for the next migration step.
- [x] Replace device-index controller pose discovery with the new pose actions, retaining device-role fallback.
- [x] Normalize SteamVR actions and legacy controller polling into one intent-named input state.
- [x] Route gameplay feedback through the new haptic actions.
- [x] Keep independent left/right targets and held-body ownership in Penumbra gameplay state.
- [x] Make R2 consume only the right-hand target and reuse hand-query collision geometry per world.
- [x] Validate initial damage/melee haptics and repeat limiting on PS VR2 Sense hardware; continue tuning individual profiles.
- [x] Add and validate default bindings for additional common SteamVR controllers.- [x] Confirm the new action path on PS VR2 hardware.
- [x] Preserve the existing OpenVR compositor while the input path changes.
- [x] Persist implemented VR settings separately from legacy desktop controls.

Controller pose validity is now refreshed every frame. A disconnected hand is
hidden and its held inputs are released instead of leaving an interactive hand
frozen at the last valid transform. Pose actions reacquire the logical left or
right hand after SteamVR reconnects it, even if its tracked-device index changes.
The recovery path was validated on PS VR2 Sense hardware on 13 August 2026. A
temporary SteamVR action-set interruption released both hands, fell back to
legacy input, and reacquired both action poses without freezing the game or the
controllers. The same session completed the VR tutorial and entered the first
Overture map.

Controller commands now cross the engine/game boundary as semantic state
(`interact`, `inventory`, `sprint`, `uiSelect`, and related intents). Vive-era
button fields remain confined to raw legacy polling and are no longer used to
transport SteamVR actions through gameplay code.

Default bindings now also ship for Valve Index (`knuckles`), Meta Quest and
Oculus Touch (`oculus_touch`), and Windows Mixed Reality
(`microsoft/motion_controller`) controllers, following the PS VR2 control
scheme. Project validation requires every default profile to cover the
mandatory actions, and `check-project.ps1`/`package.ps1` require all five
binding files. These new profiles are validated by the project checks and
SteamVR's own binding UI; hardware testing on Index, Touch, and WMR hardware
remains outstanding.

## Phase 2 — PS VR2 and comfort

- [x] Correct the PS VR2 Sense shoulder bindings and add an explicit holster action.
- [x] Place main menus and cinematics at a comfortable, headset-relative distance.
- [x] Make headset-locked main-menu/cinematic distance and scale persistent and configurable.
- [x] Expose implemented comfort and UI settings in the in-game VR options menu.
- [x] Confirm the revised controls on PS VR2 hardware.
- [x] Confirm the room-anchored menu and cinematic placement on PS VR2 hardware.
- Calibrate controller grip/aim poses and hand offsets for PS VR2 Sense.
- [x] Add a configurable radial movement dead zone, speed multiplier, and vertical height offset.
- [x] Add disabled, snap, and smooth turning through TrackingToWorld world yaw.
- [x] Add a global PS VR2 Create action for horizontal orientation recentering.
- [x] Restore gameplay crouch without changing world scale.
- [x] Add physical, button, and hybrid crouch modes with a configurable physical threshold.
- [x] Validate hybrid physical/button crouch on PS VR2 hardware.
- [x] Validate physical-only and button-only crouch on PS VR2 hardware.
- [ ] Validate low-ceiling recovery on hardware.
- [x] Lower the rendered viewpoint by the configured crouch depth (0.25 m by default) when button crouch changes the gameplay collider, instead of the deeper original move-state offset; applied through the TrackingToWorld posture offset (changed 16 August 2026, after the hardware milestone).
- [x] Calibrate a standing HMD height for physical crouch with a plausibility band (0.90–2.20 m) so an out-of-range sample (for example after loading a save) cannot become the baseline (18 August 2026; revalidated on hardware afterwards).
- Add configurable handedness and explicit seated/standing calibration.
- Add and tune controller rumble for interaction, impact, damage, and weapon events where supported on PC.
- [x] Restore the inventory action popup and add a visible tracked-controller ray and reticle.
- [x] Reuse the tracked UI ray across menus and fall back to the remaining hand after a pose loss.
- [x] Validate the shared pointer, one-hand fallback, and pose reacquisition on hardware.
- [x] Prevent rejected room-scale head movement from accumulating through solid geometry.
- [x] Preserve native character collision while manipulating swing doors in VR.
- [x] Retest the revised collision path against held locked doors and ordinary walls.
- Rework the notebook, ladders, doors, and held-object interactions for tracked controllers.

## Phase 3 — rendering and VR UX

- Audit projection, render target lifecycle, frame pacing, and mirror rendering.
- Extend configurable UI placement to the notebook and remaining in-world surfaces.
- [x] Anchor fullscreen menus/cinematics in room space and add scalable subtitles/messages.
- [x] Stage a compositor-safe black frame before synchronous map loads.
- [x] Route new-game, tutorial, and saved-game loads through the staged VR compositor path.
- [x] Remove cinematic letterboxing and subtitle backdrops while preserving scalable subtitles.
- [x] Separate normal player-physics exit from MapHandler-owned full-world reset teardown.
- [x] Enable Large Address Aware on the Win32 executable and verify the PE flag.
- [x] Force clean solution rebuilds after diagnosing stale class-layout object files.
- [x] Validate text readability, inventory pointing, and between-map black loading transitions on hardware.
- [x] Validate no-backdrop video rendering and menu/tutorial/new/continue/save loading routes on hardware.
- Add graphics and comfort presets suitable for modern headset resolutions.
- Profile CPU/GPU hot spots before attempting large rendering changes.

### Hardware milestone — 14 August 2026

The clean Release build completed repeated Steam launches and all current entry
routes: tutorial, new game, continue, and explicit save loading. Hands remained
tracked through repeated inventory transitions, controller pose loss/recovery
worked without stale input, all crouch modes drove both gameplay state and the
camera, and held locked doors could no longer be crossed by leaning and moving
forward. Menu/inventory pointing, localized action text, room-space UI,
cinematic image rendering, and staged loading were also exercised. This closes
the initial PS VR2 modernization milestone; the unchecked items above remain
future comfort, controller-coverage, and rendering work.

### VR hands — rig and basic animation (15–18 August 2026)

- [x] Rebuild the HUD hand rigs so each joint pivots at its own mesh position (joints, bind matrices, per-vertex weights).
- [x] Correct the COLLADA inverse bind matrix layout for both hands (translation into column 3); see `scripts/BINDFIX.md` and `scripts/DIAGNOSIS_FINAL.md`.
- [x] Verify single-joint, hierarchical (thumb), palm-stability, and full grip/trigger poses against an engine-exact rigid-segment simulation (`scripts/verify_bind_fix.py`).
- [x] Correct the runtime finger fold direction so both hands curl the fingers toward their own palm.
- [x] Validate both hands on PS VR2 hardware: fingers pivot at their joints, palm stays stable, thumb follows the hierarchy, grip/trigger keep working, both hands render correctly.
- [x] Assign the bone indices in both rig-construction sites so the first rig of a session also folds correctly.
- [x] Retune the fold angles and axes from bind geometry (Middle/Ring 17/17/10, Little 30/30/20, Index 17/17/10 for trigger, thumb axis from the bind plane) so fingertips stay near the palm zone; state declared **experimental final** on 18 August 2026 and intentionally frozen for now.
- [x] Fix the flare `.hud` so the flare renders at the right VR scale/orientation.

Status: the hand animation work is frozen as an experimental state, not a
finished feature, and the current fold state still shows incorrect visual
behaviours. The final angle set (18 August) has no recorded full
hardware-validation pass after the retuning; revalidation is expected whenever
this area is touched again.

Hand rig tooling (in `scripts/`):

- `verify_bind_fix.py` — engine-exact simulation of the HPL1 skin (IBM layout,
  per-joint rotations, wrap-around tolerance). Run after any change to the hand
  rig `.dae` files to confirm joints stay at their mesh positions.
- `fix_rig_bind_layout.py` and `fix_rig_weights.py` — the two applied one-shot
  fixes (bind-matrix layout, vertex weights). Idempotent; use `--dry-run` to
  preview and `--apply` to write. Originals are backed up under
  `.penumbravr/backup`.
- `make_hand_rig.py` — regenerates the rigged (skinned) hand models from the
  static meshes.
- `render_hand.py` — renders bind vs. grip comparison images
  (`render_0/1/2.png`).
- `analyze_other_fingers.py` (uses `diag_rig_geometry.py`) — geometric analysis
  of the finger flexion axes and tip trajectories; use when re-tuning fold
  angles or axes.
- The full diagnosis history is recorded in `scripts/DIAGNOSIS.md`,
  `scripts/DIAGNOSIS_FINAL.md`, `scripts/BINDFIX.md`, and `scripts/WEIGHTFIX.md`.

Known residual issues:

- Ring/Middle share one fused tube in the source geometry, so their tips cannot spread independently.
- The Index has a limited free tube between the palm and its first joint, which restricts its clean bend range.
- The thumb chain is short relative to the palm; its tip does not reach the palm flesh (model limitation).
- Poses are still basic (grip/trigger blends only); per-finger curl shaping and finer grip profiles are future work.

## Phase 4 — OpenXR evaluation

- Implement an OpenXR backend behind the runtime boundary.
- Compare it with the stable SteamVR/OpenVR path on PS VR2 and other headsets.
- Make OpenXR the default only after feature parity and regression testing.
