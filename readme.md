# Penumbra: Overture VR Rework

<p align="center">
  <a href="HPL1Engine/COPYING"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
  <img alt="Status: stable" src="https://img.shields.io/badge/status-stable-brightgreen?style=flat-square">
  <a href="https://github.com/rubocopter/penumbra_vr_rework/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/rubocopter/penumbra_vr_rework?style=flat-square&label=release"></a>
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**Play Penumbra: Overture in full room-scale PCVR, with motion-controlled hands, physical interaction and spatial audio.**

> **v0.1.0 — first stable release.** Requires your own copy of **Penumbra: Overture**. The original game is not included.

[**Download v0.1.0**](https://github.com/rubocopter/penumbra_vr_rework/releases/tag/v0.1.0) · [Controls & VR settings](docs/INPUT.md) · [Troubleshooting](docs/TROUBLESHOOTING.md) · [Roadmap](docs/ROADMAP.md)

## Highlights

- Full room-scale VR: walk, crouch, lean and reach naturally.
- Motion-controlled hands with animated fingers and collision-constrained palms.
- Physical interaction with objects, doors, drawers, hatches and mechanisms.
- Assisted acquisition for visible inventory items that are difficult to reach.
- Standing and seated play, height calibration and left- or right-handed controls.
- Snap, smooth and physical-only turning, with configurable movement and crouch modes.
- Binaural HRTF audio, distance absorption, occlusion and ambient reverb.
- Performance, Balanced and Quality presets with adjustable render scale.
- Optional Enhanced visuals and 231 reviewed diffuse texture replacements.
- SteamVR profiles for PS VR2 Sense, Valve Index, Meta Quest/Touch, Pico, WMR and HTC Vive.
- Room-anchored menus and cinematics with scalable subtitles.
- Reversible installer that backs up every original file it replaces.
- Spanish localization for the VR tutorial, menus and controller notes.

## Quick start

1. Download the release `.zip` and extract it completely.
2. Run `Install-PenumbraVR.bat`; it locates the Steam installation and backs up replaced files.
3. Start SteamVR and launch **Penumbra: Overture** normally from Steam.

Requires Windows 10/11, SteamVR and a PCVR headset. The Visual C++ runtime is included in the package. To remove the mod, run `Install-PenumbraVR.bat -Restore`.

The texture pack can be omitted with `Install-PenumbraVR.bat -SkipTexturePack`. See [texture credits and permissions](docs/TEXTURE_CREDITS.md) for provenance and redistribution terms.

## Status

**v0.1.0 is the stable baseline.** The full game is playable in VR, and the main tracking, interaction, collision, comfort, UI, audio, installation and shutdown paths have automated and PS VR2 hardware validation.

PS VR2 Sense is currently the only controller family tested end to end on physical hardware. Index, Touch, Pico, WMR and Vive profiles are bundled but still need device-specific reports. Known limitations and validation details are tracked in the [release history](docs/RELEASES.md), [validation record](docs/VALIDATION-v0.1.0.md) and [roadmap](docs/ROADMAP.md).

The newer [Penumbra VR Framework](https://github.com/rubocopter/penumbra_vr_framework) is extending the proven work from this Rework toward **Black Plague** and **Requiem**. This repository remains the playable Overture implementation and behavioral reference.

## Spatial audio

The audio path is built around a bundled OpenAL Soft runtime and includes head-tracked positional audio, optional binaural HRTF, distance absorption, geometry-aware occlusion and restrained ambient reverb. The goal is not just positional sound: walls, distance and the shape of the environment affect how threats are perceived.

## Documentation

- [Controls and VR settings](docs/INPUT.md)
- [Troubleshooting and compatibility](docs/TROUBLESHOOTING.md)
- [VR architecture](docs/VR_ARCHITECTURE.md)
- [Input architecture](docs/INPUT_ARCHITECTURE.md)
- [Rendering and lighting research](docs/LIGHTING.md)
- [Texture selection](docs/TEXTURES.md)
- [Stable validation](docs/VALIDATION-v0.1.0.md)
- [Roadmap](docs/ROADMAP.md)
- [Release history](docs/RELEASES.md)

## Development

`scripts/build.ps1` validates the project, builds Win32, runs tests and can package or deploy a release layout. SteamVR bindings are generated from the project specification with `scripts/generate-bindings.ps1`, and CI builds/packages Win32 on pushes, pull requests and tags.

Development artifacts are available through [GitHub Actions](https://github.com/rubocopter/penumbra_vr_rework/actions/workflows/build.yml); they are not public releases.

## Support

If you enjoy the project and would like to support its continued development, you can [support me on Ko-fi](https://ko-fi.com/onitaku).

## About and license

This is an unofficial community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr). It is not affiliated with Frictional Games, Valve or Sony Interactive Entertainment.

Licensed under GPL v3 or later. See `HPL1Engine/COPYING` and `PenumbraOverture/COPYING` for details and third-party notices. Externally supplied textures retain their own [attribution and permissions](docs/TEXTURE_CREDITS.md).
