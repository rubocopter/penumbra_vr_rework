# Modernization roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

## Phase 0 — reproducible baseline

- Build all three native projects with Visual Studio 2022.
- Remove author-specific paths and keep generated files outside source directories.
- Produce a deterministic package layout with checksums.
- Build the same target in GitHub Actions.
- Confirm that the historical executable starts against an unmodified Steam installation.

## Phase 1 — runtime and input boundary

- Introduce device-independent head, hand, action, and haptics interfaces.
- Replace direct Vive button/axis polling with SteamVR Input actions.
- Add explicit default bindings for PS VR2 Sense and common SteamVR controllers.
- Preserve the existing OpenVR compositor while the input path changes.

## Phase 2 — PS VR2 and comfort

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
