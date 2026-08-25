# Penumbra: Overture VR Rework

A community-made PCVR rework that brings **Penumbra: Overture** to full room-scale VR with motion-controlled hands, physical interactions, and a complete spatial-audio chain built for horror.

> Experimental public alpha. You must own **Penumbra: Overture** on Steam; the original game is not included.

**[Download the latest release](https://github.com/rubocopter/penumbra_vr_rework/releases/latest)** · [Controls and VR settings](docs/INPUT.md)

## Why try it?

- Full room-scale PCVR — walk, crouch, lean, reach
- Motion-controlled hands with animated fingers and visible equipped objects
- Physical interaction — pick up, throw, and manipulate objects naturally
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

Requires Windows 10/11, SteamVR, a PCVR headset, and the Microsoft Visual C++ 2015–2022 Redistributable (x86). To remove the mod, run `Install-PenumbraVR.bat -Restore`; only files owned by the mod are removed, and backed-up originals are restored.

## Status

**Experimental public alpha.** The game is playable end-to-end, but expect bugs, crashes, and incorrect behaviour. Controller coverage and hand rendering remain under active validation; see the [release history](docs/RELEASES.md) for build-specific status.

## Documentation

- [Controls and VR settings](docs/INPUT.md) — bindings, comfort options, compatibility, and diagnostics
- [VR architecture](docs/VR_ARCHITECTURE.md) — tracking space, world placement, UI, and hands
- [Input architecture](docs/INPUT_ARCHITECTURE.md) — input flow and the engine/game boundary
- [Roadmap](docs/ROADMAP.md) — current priorities and future work
- [Release history](docs/RELEASES.md) — changes and known limitations by version

## About

This is a community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr). It is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment.

## License

Licensed under GPL v3 or later. See `HPL1Engine/COPYING` and `PenumbraOverture/COPYING` for details and third-party notices.
