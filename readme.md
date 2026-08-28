# Penumbra: Overture VR Rework

A community-made PCVR rework that brings **Penumbra: Overture** to full room-scale VR with motion-controlled hands, physical interactions, and a complete spatial-audio chain built for horror.

> Experimental public alpha — current release: **v0.1.0-alpha.6.3** (28 August 2026). You must own **Penumbra: Overture** on Steam; the original game is not included.

**[Download the latest release](https://github.com/rubocopter/penumbra_vr_rework/releases/latest)** · [Controls and VR settings](docs/INPUT.md)

## Why try it?

- Full room-scale PCVR — walk, crouch, lean, reach
- Motion-controlled hands with animated fingers, collision-constrained palms, and geometry-calibrated equipped objects
- Physical interaction — pick up, throw, nudge, and manipulate objects, doors, drawers, hatches, and mechanisms naturally
- Assisted magnetic acquisition for visible inventory items that are difficult to reach, with occlusion checks and per-hand feedback
- Low-ceiling crouch recovery that preserves the requested physical, button, or hybrid stance until standing is possible again
- **Hear the dark**: binaural HRTF audio, creatures that sound muffled through walls, distant sources that lose their treble, and a mine-gallery reverb that makes corridors breathe
- Standing or seated play, left- or right-handed controls, and height calibration
- Snap, smooth, or physical-only turning plus adjustable movement and crouch modes
- Performance, Balanced, and Quality presets with adjustable render scale
- SteamVR profiles for PS VR2 Sense, Valve Index, Meta Quest/Touch, Pico, WMR, and HTC Vive
- Room-anchored menus and cinematics with scalable subtitles (no letterboxing)
- Reversible installer that backs up every original file it replaces
- Spanish localization for VR tutorial, menus, and controller notes

## Spatial audio

Penumbra is about what you hear coming. This rework rebuilds the entire acoustic chain on top of a bundled OpenAL Soft runtime, so it behaves identically on every machine:

1. **Positional tracking** — the listener follows your head pose in real time, not last frame's render matrix
2. **Binaural HRTF** — optional headphone spatialization with elevation cues, set to Auto, Headphones, or Off in VR Settings
3. **Distance absorption** — far sources dull progressively even in line of sight
4. **Occlusion** — walls and closed doors muffle treble with a smooth fade, so a creature behind a door sounds held back until you open it
5. **Ambient reverb** — a restrained stone-gallery tail that gives large spaces depth without washing out the mix

## Quick start

1. Download the release `.zip` and extract it completely.
2. Run `Install-PenumbraVR.bat`; it finds the Steam installation and backs up replaced files.
3. Start SteamVR, then launch **Penumbra: Overture** with Steam's normal **Play** button.

Requires Windows 10/11, SteamVR, and a PCVR headset — the Visual C++ runtime ships inside the package since alpha.6.1. To remove the mod, run `Install-PenumbraVR.bat -Restore`; only files owned by the mod are removed, and backed-up originals are restored.

## Status

**v0.1.0-alpha.6.3 — experimental public alpha.** The game is playable end-to-end, but expect bugs, crashes, and incorrect behaviour. PS VR2 Sense is the only controller family validated on hardware; profiles for Index, Touch, Pico, WMR, and Vive are bundled but still require device-specific testing. Hand collision uses an approximate palm volume rather than per-finger physics, the current hand mesh has limited thumb and ring/middle movement, and the original game state still permits only one active grab. See the [release history](docs/RELEASES.md) for build-specific status.

## Documentation

- [Troubleshooting and compatibility](docs/TROUBLESHOOTING.md) — crash dumps and logs, laptop GPUs, audio devices, controller resets, non-Steam installs
- [Controls and VR settings](docs/INPUT.md) — bindings, comfort options, compatibility, and diagnostics
- [VR architecture](docs/VR_ARCHITECTURE.md) — tracking space, world placement, UI, and hands
- [Input architecture](docs/INPUT_ARCHITECTURE.md) — input flow and the engine/game boundary
- [Roadmap](docs/ROADMAP.md) — current priorities and future work
- [Release history](docs/RELEASES.md) — changes and known limitations by version

## About

This is a community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr). It is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment.

## Development

- `scripts/build.ps1` — validates the project, builds Win32, runs the unit tests, and (with `-Package` / `-Deploy`) produces or installs a release layout
- `scripts/generate-bindings.ps1` — regenerates the SteamVR controller bindings; the build fails if the shipped JSONs drift from this spec
- `tests/VRTrackingTest` — unit tests for the pure VR math, executed by every build
- `.github/workflows/build.yml` — CI compiles and packages Win32 on every push, PR, and tag

When working from a source checkout, deploy with `scripts/build.ps1 -Deploy`. The packaged release places `Install-PenumbraVR.bat` in its root; the repository copy lives under `scripts/`.

## License

Licensed under GPL v3 or later. See `HPL1Engine/COPYING` and `PenumbraOverture/COPYING` for details and third-party notices.
