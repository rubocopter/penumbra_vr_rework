# Roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

## Current focus

- Hand-rig polish — thumb behaviour, fused ring/middle mesh, finger splay, and any remaining item-specific calibration
- Two-hand interaction — simultaneous object ownership and optional two-point constraints for large props
- Map-wide mechanism validation — unusual doors, wheels, levers, ladders, and constrained props beyond the opening area
- Controller coverage — hardware testing beyond PS VR2 Sense and follow-up on early Index feedback
- Performance — CPU/GPU profiling and sensible defaults for modern headset resolutions

## Next

- Decide whether the current hand asset can support a convincing thumb wrap or should be replaced
- Validate Index, Touch, Pico, WMR, and Vive profiles on real hardware
- Refine notebook, ladder, door, and held-object interactions for tracked controllers
- Design a two-hand state model without breaking the original single-state interaction callbacks
- Low-ceiling recovery for physical crouch
- Prototype an OpenXR backend behind the existing runtime boundary and compare it with the SteamVR path

## Future

- Make OpenXR the default only after feature parity and regression testing
- Replace or substantially rework the hand asset if independent ring/middle motion, thumb wrap, and finger splay cannot be achieved with the current mesh
- Per-zone or map-driven reverb environments on top of the global mine preset
- Custom HRTF dataset selection alongside the OpenAL Soft built-in set

## Completed

- SteamVR Input abstraction with device-independent actions and legacy fallback
- PS VR2 Sense support and hardware testing — gameplay, UI, haptics, bindings
- Room-scale VR — TrackingToWorld boundary, recenter, snap/smooth/disabled turn
- Persistent VR settings — in-game editor under Options → VR Settings
- Physical, button, and hybrid crouch with standing-height calibration
- Room-anchored menus & cinematics with scalable subtitles (no letterboxing)
- Compositor-safe black loading frames for all map transitions
- Reversible installer — manifest-owned sync, backup/restore, Steam launch retention
- Spanish localization for VR tutorial, menus, controller notes
- Controller profiles for Index, Touch, Pico, WMR, Vive (bundled, unvalidated)
- Input resilience — 500 ms dropout bridging, legacy movement from strongest axis
- Graphics presets (Performance/Balanced/Quality) and render scale setting
- Equipped objects in hand — geometry-calibrated grip points, radius-driven finger curl, per-item twist, with melee and throwing preserved
- Left-handed mirror — fully mirrored action sets, equipment slots, notebook on off hand
- Bare off-hand targeting and grabbing on PS VR2 Sense
- Collision-constrained visible hands — swept approximate hand volume, surface sliding, bounded visual lag, and independent tracking-loss recovery
- Adaptive hand nudge for physics props, inventory pickups, hinges, and sliders; stronger for small/light objects and restrained for heavy or breakable props
- Joint-aware VR mechanisms — actual driven joint selection, circular hinge drag, slider projection, easier hatches/lockers, inserted-lever forwarding, and locked-door axis retention
- Low-latency VR audio listener — OpenAL orientation/position built from fresh head tracking data each update instead of the previous render's camera matrix
- Spatial-audio chain on bundled OpenAL Soft 1.25.2: binaural HRTF toggle (Auto/Headphones/Off), global mine-gallery reverb, occlusion muffling through per-source low-pass filters, and distance treble absorption
- Startup crash hardening — legacy sound-updater thread force-disabled on upgraded installs, typed on-demand EFX filters, eye-framebuffer completeness checks with half-size retries
- Automatic crash reporting — minidump plus exception line written by the game itself, no event-viewer archaeology needed
- Environment fingerprinting in `hpl.log` — Windows build, RAM and free process address space, real GPU/driver, audio devices
- Stale-binding diagnostics for dead sticks with the documented one-time SteamVR reset
- Generated controller bindings — single declarative spec in `scripts/generate-bindings.ps1` with build-time drift enforcement
- Unit tests for the pure VR math (tracking space, stick dead zones) executed by every build
- CI pipeline building and packaging Release Win32 on every push, PR, and tag
