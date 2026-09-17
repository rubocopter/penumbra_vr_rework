# Penumbra: Overture VR Rework

<p align="center">
  <a href="HPL1Engine/COPYING"><img alt="License: GPL v3+" src="https://img.shields.io/badge/license-GPL%20v3%2B-blue?style=flat-square"></a>
  <img alt="Status: stable" src="https://img.shields.io/badge/status-stable-brightgreen?style=flat-square">
  <a href="https://github.com/rubocopter/penumbra_vr_rework/releases/latest"><img alt="Latest release" src="https://img.shields.io/github/v/release/rubocopter/penumbra_vr_rework?style=flat-square&label=release"></a>
</p>
<p align="center">
  <a href="https://ko-fi.com/onitaku"><img alt="Support me on Ko-fi" src="https://ko-fi.com/img/githubbutton_sm.svg"></a>
</p>

**Play Penumbra: Overture in full room-scale PCVR with motion-controlled hands, physical interaction and spatial audio.**

> **v0.1.0 is the current stable release.** You need your own copy of **Penumbra: Overture**; the game itself is not included.

[**Download v0.1.0**](https://github.com/rubocopter/penumbra_vr_rework/releases/tag/v0.1.0) · [Controls & settings](docs/INPUT.md) · [Troubleshooting](docs/TROUBLESHOOTING.md) · [Roadmap](docs/ROADMAP.md)

## Highlights

- Full room-scale movement: walk, crouch, lean and reach naturally.
- Motion-controlled hands with animated fingers and physical interaction.
- Doors, drawers, hatches, objects and mechanisms adapted for VR.
- Standing/seated play, height calibration and left/right-handed controls.
- Snap, smooth and physical-only turning plus configurable movement modes.
- Binaural HRTF audio with distance, occlusion and ambient reverb.
- Performance, Balanced and Quality presets plus optional enhanced visuals.
- SteamVR bindings for PS VR2 Sense, Index, Touch, Pico, WMR and Vive.
- Room-anchored menus/cinematics and scalable subtitles.
- Reversible installer with backups of replaced files.

## Quick start

1. Download and fully extract the release `.zip`.
2. Run `Install-PenumbraVR.bat` and let it locate/back up your Steam installation.
3. Start SteamVR and launch **Penumbra: Overture** normally from Steam.

Requires Windows 10/11, SteamVR and a PCVR headset. Restore the original installation at any time with `Install-PenumbraVR.bat -Restore`.

PS VR2 Sense is the controller family currently validated end to end on physical hardware. Other bundled profiles still benefit from device-specific reports.

## Project status

The full game is playable in VR and the main tracking, interaction, collision, comfort, UI, audio, installation and shutdown paths have automated and PS VR2 hardware validation.

The newer [Penumbra VR Framework](https://github.com/rubocopter/penumbra_vr_framework) is carrying this work forward toward **Black Plague** and **Requiem**. This repository remains the playable Overture implementation and behavioral reference.

## Documentation

[Controls](docs/INPUT.md) · [Troubleshooting](docs/TROUBLESHOOTING.md) · [Validation](docs/VALIDATION-v0.1.0.md) · [Release history](docs/RELEASES.md) · [VR architecture](docs/VR_ARCHITECTURE.md) · [Input architecture](docs/INPUT_ARCHITECTURE.md) · [Texture credits](docs/TEXTURE_CREDITS.md)

<details>
<summary><strong>Development</strong></summary>

`scripts/build.ps1` validates the project, builds Win32, runs tests and can package/deploy a release layout. SteamVR bindings are generated from the project specification with `scripts/generate-bindings.ps1`; CI builds and packages Win32 on pushes, pull requests and tags.

[Development artifacts](https://github.com/rubocopter/penumbra_vr_rework/actions/workflows/build.yml) are not public releases.

</details>

## About and license

This is an unofficial community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr). It is not affiliated with Frictional Games, Valve or Sony Interactive Entertainment.

Licensed under GPL v3 or later. See `HPL1Engine/COPYING`, `PenumbraOverture/COPYING` and the [texture credits](docs/TEXTURE_CREDITS.md) for third-party terms.
