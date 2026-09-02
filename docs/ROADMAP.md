# Roadmap

The rework is intentionally incremental. Every phase should leave a testable build so regressions can be attributed to a small set of changes.

## Current focus

- Hand-rig polish — thumb behaviour, fused ring/middle mesh, finger splay, and any remaining item-specific calibration
- Two-hand interaction — simultaneous object ownership and optional two-point constraints for large props
- Map-wide mechanism validation — unusual doors, wheels, levers, ladders, and constrained props beyond the opening area
- Controller coverage — hardware testing beyond PS VR2 Sense and follow-up on early Index feedback
- Performance — CPU/GPU profiling and sensible defaults for modern headset resolutions
- Graphics evaluation — v4 received positive headset feedback and remains the visual baseline. The complete supplied texture pack has now been audited and a single 231-image bounded-resolution batch prepared. Its grouped hardware test, memory observation and matched GPU timings remain open; provenance, decisions and rollback are documented

## Next-beta checkpoint

Keep the current work under **Unreleased** while committing and pushing tested
checkpoints. Publishing source and CI packages does not require publishing a
new release or moving an existing tag.

Recommended bounded scope for the next beta:

1. Preserve v4 as the visual baseline, with Enhanced visuals Off by default and
   the original renderer/fallback intact. Do not reopen the discarded SSAO test.
2. Test the prepared 231-image batch as one group. The expansion decision is
   now made; do not keep adding textures or lighting changes before this test.
   The original nine remain unchanged within it.
3. Complete a short grouped regression run: lit office and dark cave, flashlight
   and glowstick, On/Off at fixed scale, shadow silhouettes and map transitions.
   Record per-eye timings and process-memory headroom, and verify installation
   and texture-only rollback for the final package.
4. Publish the next beta after that scope is frozen and the checks pass. Keep
   larger textures and further lighting experiments for a later checkpoint.

## Next

- Decide whether the current hand asset can support a convincing thumb wrap or should be replaced
- Validate Index, Touch, Pico, WMR, and Vive profiles on real hardware
- Refine notebook, ladder, door, and held-object interactions for tracked controllers
- Design a two-hand state model without breaking the original single-state interaction callbacks
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
- Interaction-aware hand contact — full-size palm collision, separate translation/rotation sweeps, refined surface sliding, bounded target contact skin, and body-side recovery for load/hinge/ceiling overlaps without crossing barriers
- Adaptive hand nudge for physics props, inventory pickups, hinges, and sliders; stronger for small/light objects and restrained for heavy or breakable props
- Joint-aware VR mechanisms — actual driven joint selection, circular hinge drag, slider projection, easier hatches/lockers, inserted-lever forwarding, and locked-door axis retention
- Low-ceiling crouch constraint — full-height collider retry with automatic restoration of the physical/button/hybrid requested stance
- Assisted hand-aim acquisition for visible inventory pickups, with distance/angle ranking, controller-plus-head occlusion, per-hand haptic feedback, and an isolated blue object-to-middle-finger world cue
- Low-latency VR audio listener — OpenAL orientation/position built from fresh head tracking data each update instead of the previous render's camera matrix
- Spatial-audio chain on bundled OpenAL Soft 1.25.2: binaural HRTF toggle (Auto/Headphones/Off), global mine-gallery reverb, occlusion muffling through per-source low-pass filters, and distance treble absorption
- Startup crash hardening — legacy sound-updater thread force-disabled on upgraded installs, typed on-demand EFX filters, eye-framebuffer completeness checks with half-size retries
- Automatic crash reporting — minidump plus exception line written by the game itself, no event-viewer archaeology needed
- Environment fingerprinting in `hpl.log` — Windows build, RAM and free process address space, real GPU/driver, audio devices
- Stale-binding diagnostics for dead sticks with the documented one-time SteamVR reset
- Generated controller bindings — single declarative spec in `scripts/generate-bindings.ps1` with build-time drift enforcement
- Unit tests for tracking space, stick dead zones, VR hand-collision policy, high-speed wall/closed-door blocking, contact rotation, and constrained recovery executed by every build
- CI pipeline building and packaging Release Win32 on every push, PR, and tag
- VR Settings accessibility layout — two columns keep every option and the Back button within the cursor's 800×600 selectable area
- HPL1 lighting audit and minimal half-resolution SSAO feasibility test — depth/stencil sampling, per-eye reconstruction, ambient-only composition and GPU timing were validated; the experiment was then fully removed because its roughly 0.46 ms stereo cost produced negligible visible benefit
- Enhanced-ambient feasibility A/B — dedicated BaseLight `RenderZ()` shaders established a hot, monitor-isolated alternative path; two headset tests found the ambient-only result too subtle, so it is now one component of the broader Enhanced visuals comparison rather than a standalone proposal
