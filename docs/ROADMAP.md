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

Baseline validation was completed on 13 August 2026. The Release build launched through SteamVR on PS VR2, retained the historical mod's behaviour, and loaded the VR tutorial in Spanish. The normal Steam installation remained untouched. Missing Spanish menu keys in the legacy translation had previously resolved to empty strings, hiding the VR tutorial option; the English and Spanish key sets are now kept in sync by project validation.

## Phase 1 — runtime and input boundary

- [x] Route HMD and controller world poses through one explicit TrackingToWorld transform.
- [x] Update the bundled OpenVR SDK and Win32 loader from 1.0.2 to 2.15.6.
- [x] Introduce device-independent gameplay and UI action sets with an automatic legacy fallback.
- [x] Add explicit default bindings for PS VR2 Sense and HTC Vive controllers.
- [x] Expose device-independent pose and haptic actions for the next migration step.
- [x] Replace device-index controller pose discovery with the new pose actions, retaining device-role fallback.
- [ ] Route gameplay feedback through the new haptic actions.
- [ ] Add and validate default bindings for additional common SteamVR controllers.
- [x] Confirm the new action path on PS VR2 hardware.
- [x] Preserve the existing OpenVR compositor while the input path changes.

Controller pose validity is now refreshed every frame. A disconnected hand is
hidden and its held inputs are released instead of leaving an interactive hand
frozen at the last valid transform. Pose actions reacquire the logical left or
right hand after SteamVR reconnects it, even if its tracked-device index changes.
The recovery path was validated on PS VR2 Sense hardware on 13 August 2026. A
temporary SteamVR action-set interruption released both hands, fell back to
legacy input, and reacquired both action poses without freezing the game or the
controllers. The same session completed the VR tutorial and entered the first
Overture map.

## Phase 2 — PS VR2 and comfort

- [x] Correct the PS VR2 Sense shoulder bindings and add an explicit holster action.
- [x] Place main menus and cinematics at a comfortable, headset-relative distance.
- [x] Confirm the revised controls on PS VR2 hardware.
- [ ] Confirm the revised menu and cinematic placement on PS VR2 hardware.
- Calibrate controller grip/aim poses and hand offsets for PS VR2 Sense.
- Add smooth and snap turning, configurable handedness, dead zones, seated/standing height, and recentering.
- Add controller rumble for interaction, impact, damage, and weapon events where supported on PC.
- Rework menus, inventory, notebook, ladders, doors, and held-object interactions for tracked controllers.

## Phase 3 — rendering and VR UX

- Audit projection, render target lifecycle, frame pacing, and mirror rendering.
- Fix UI scale, stereo depth, text readability, loading screens, and cinematics.
- Add graphics and comfort presets suitable for modern headset resolutions.
- Profile CPU/GPU hot spots before attempting large rendering changes.

## Phase 4 — OpenXR evaluation

- Implement an OpenXR backend behind the runtime boundary.
- Compare it with the stable SteamVR/OpenVR path on PS VR2 and other headsets.
- Make OpenXR the default only after feature parity and regression testing.
