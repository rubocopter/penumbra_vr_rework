# Modernization roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

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
- [ ] Add and validate default bindings for additional common SteamVR controllers.
- [x] Confirm the new action path on PS VR2 hardware.
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
- [x] Lower the rendered viewpoint smoothly when button crouch changes the gameplay collider.
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

## Phase 4 — OpenXR evaluation

- Implement an OpenXR backend behind the runtime boundary.
- Compare it with the stable SteamVR/OpenVR path on PS VR2 and other headsets.
- Make OpenXR the default only after feature parity and regression testing.
