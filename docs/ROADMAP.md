# Roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

## Current focus

- Hand and equipped-object polish — fist wrap, thumb behaviour, and per-item placement
- Two-hand interaction — simultaneous object ownership and optional two-point constraints for large props
- Controller coverage — hardware testing beyond PS VR2 Sense and follow-up on early Index feedback
- Performance — CPU/GPU profiling and sensible defaults for modern headset resolutions

## Next

- Improve grip poses and per-item alignment without destabilizing the current rig
- Validate Index, Touch, Pico, WMR, and Vive profiles on real hardware
- Refine notebook, ladder, door, and held-object interactions for tracked controllers
- Design a two-hand state model without breaking the original single-state interaction callbacks
- Low-ceiling recovery for physical crouch
- Prototype an OpenXR backend behind the existing runtime boundary and compare it with the SteamVR path

## Future

- Make OpenXR the default only after feature parity and regression testing
- Replace or substantially rework the hand asset if independent ring/middle motion, thumb wrap, and finger splay cannot be achieved with the current mesh

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
- Equipped objects in hand — first experimental pass with melee and throwing preserved
- Left-handed mirror — fully mirrored action sets, equipment slots, notebook on off hand
- Bare off-hand targeting and grabbing on PS VR2 Sense
- Dynamic hand nudge for physics props and hinged doors, excluding inventory items
