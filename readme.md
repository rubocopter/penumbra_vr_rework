# Penumbra: Overture VR Rework

Community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the experimental VR port of Frictional Games' Penumbra: Overture.

This fork is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment. It does not include the commercial game. You must own Penumbra: Overture to use it.

## Project status

The `modernization` branch now has a reproducible Windows build and has started migrating runtime input. It uses OpenVR 2.15.6, SteamVR Input action sets with native PS VR2 Sense and legacy HTC Vive bindings, logical left/right pose actions, and an automatic fallback to the historical controller polling and device-role paths. Rendering still uses the proven OpenVR compositor path, and the engine remains 32-bit because of its legacy binary dependencies.

The initial compatibility target is SteamVR with PS VR2 on PC. Native OpenXR support is a later milestone; keeping OpenVR for the first working baseline reduces the number of systems changed at once. See [the current controller bindings](docs/INPUT.md) for the default layout and testing notes.

See [the modernization roadmap](docs/ROADMAP.md) for the planned stages and
[the VR architecture](docs/VR_ARCHITECTURE.md) for the tracking, world-space,
height, hand-state, and gameplay boundaries. The current boundary between
SteamVR, HPL1 input, and Penumbra gameplay is mapped in
[the input architecture](docs/INPUT_ARCHITECTURE.md).

## Build

Requirements:

- Windows 10 or 11 (64-bit host).
- Visual Studio 2022 or Visual Studio 2022 Build Tools.
- The **Desktop development with C++** workload, including the MSVC v143 x86/x64 tools and a Windows 10 or 11 SDK.
- PowerShell 7.

Build and create a redistributable mod package:

```powershell
./scripts/build.ps1 -Configuration Release -Package
```

Build, package, and synchronize a development build into the detected Steam
installation:

```powershell
./scripts/build.ps1 -Configuration Release -Deploy
```

`-Deploy` is deliberately explicit. It records every mod-owned path under
`.penumbravr/deploy-state.json`, backs up pre-existing files, removes only stale
files recorded by an earlier deployment, and never mirrors or deletes the rest
of the commercial game's `redist` directory. By default it also backs up and
replaces `redist/Penumbra.exe`, allowing Steam's **Play** button to launch the VR
build and retain Steam session tracking. Pass `-NoSteamLauncher` to leave the
flat executable active, or `-InstallRoot 'X:\...\Penumbra Overture'` for a
non-default Steam library.

Restore every backed-up original and remove deployed mod-only files with:

```powershell
./scripts/deploy.ps1 -Restore
```

The build remains Win32 intentionally because the checked-in Newton, FLTK, SDL, OpenAL, Cg, and other third-party libraries are 32-bit binaries. Build products are written under `build/`; source directories are not modified.

The package is created at:

```text
build/package/Release/PenumbraVR
```

## Install a development build

1. Install Penumbra: Overture through Steam.
2. If another installer has modified the Steam copy, verify the game files first.
3. Run `./scripts/build.ps1 -Configuration Release -Deploy` from a source checkout.
4. Install and configure SteamVR. PS VR2 users also need Sony's PlayStation VR2 App and the required PC hardware.
5. Install the Microsoft Visual C++ 2015–2022 Redistributable (x86) if it is not already present.
6. Start SteamVR and launch **Penumbra: Overture** with Steam's normal **Play** button.

A packaged release includes `Install-PenumbraVR.ps1` for the same reversible
installation flow without a source checkout. Manual copying remains possible,
but cannot remove stale mod files or restore originals automatically. The
package's `config`, `maps`, and `models` folders must merge directly under
`redist`; never create a `redist/data` directory.

The package only contains this project's executable, its OpenVR loader, modified open-source/game-patch data already present in the upstream repository, documentation, and checksums. It does not bundle the base game.

### Spanish language

The package includes `config/Espanol.lang`. Select **Español** under **Options → Game → Language**. The VR tutorial, VR menu options, and controller notes have been translated for this rework.

The base Spanish localization originated with the official Penumbra: Overture translation, was prepared for later Windows and Linux digital releases by DeM, and was revised by Oscar “darkpadawan” Rodríguez for [Clan DLAN](https://www.clandlan.net/). This project adds only the VR-specific strings. The accompanying translation readme did not state explicit redistribution terms, so permission should be confirmed before publishing a formal release containing the full language file.

## Original project

The original mod added head and hand tracking for the HTC Vive. Its last upstream commit was published in 2018. Historical releases and preview material remain available through the links in the upstream repository.

## License

The HPL1 engine and Penumbra: Overture source in this repository are distributed under GNU GPL v3 or later; see `HPL1Engine/COPYING` and `PenumbraOverture/COPYING`. OALWrapper and bundled third-party components retain their respective notices and licenses. Modified source distributions and releases must preserve those notices and provide the corresponding source as required.
