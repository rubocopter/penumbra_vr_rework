# Penumbra: Overture VR Rework

Community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the experimental VR port of Frictional Games' Penumbra: Overture.

This fork is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment. It does not include the commercial game. You must own Penumbra: Overture to use it.

## Project status

The `modernization` branch is currently restoring a reproducible Windows build before changing runtime behaviour. The historical implementation still uses OpenVR 1.0.2, legacy controller polling designed around HTC Vive wands, and 32-bit engine dependencies.

The initial compatibility target is SteamVR with PS VR2 on PC. Native OpenXR support is a later milestone; keeping OpenVR for the first working baseline reduces the number of systems changed at once.

See [the modernization roadmap](docs/ROADMAP.md) for the planned stages.

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

The build remains Win32 intentionally because the checked-in Newton, FLTK, SDL, OpenAL, Cg, and other third-party libraries are 32-bit binaries. Build products are written under `build/`; source directories are not modified.

The package is created at:

```text
build/package/Release/PenumbraVR
```

## Install a development build

1. Install Penumbra: Overture through Steam.
2. Back up saves and any files previously installed by another mod version.
3. Copy the contents of the generated `PenumbraVR` package into the game's `redist` directory. The package's `config`, `maps`, and `models` folders must merge with the folders of the same name directly under `redist`; do not create a `redist/data` directory.
4. Install and configure SteamVR. PS VR2 users also need Sony's PlayStation VR2 App and the required PC hardware.
5. Install the Microsoft Visual C++ 2015–2022 Redistributable (x86) if it is not already present.
6. Start SteamVR, then run `Penumbra_vr.exe` from the game directory.

The package only contains this project's executable, its OpenVR loader, modified open-source/game-patch data already present in the upstream repository, documentation, and checksums. It does not bundle the base game.

### Spanish language

The package includes `config/Espanol.lang`. Select **Español** under **Options → Game → Language**. The VR tutorial, VR menu options, and controller notes have been translated for this rework.

The base Spanish localization originated with the official Penumbra: Overture translation, was prepared for later Windows and Linux digital releases by DeM, and was revised by Oscar “darkpadawan” Rodríguez for [Clan DLAN](https://www.clandlan.net/). This project adds only the VR-specific strings. The accompanying translation readme did not state explicit redistribution terms, so permission should be confirmed before publishing a formal release containing the full language file.

## Original project

The original mod added head and hand tracking for the HTC Vive. Its last upstream commit was published in 2018. Historical releases and preview material remain available through the links in the upstream repository.

## License

The HPL1 engine and Penumbra: Overture source in this repository are distributed under GNU GPL v3 or later; see `HPL1Engine/COPYING` and `PenumbraOverture/COPYING`. OALWrapper and bundled third-party components retain their respective notices and licenses. Modified source distributions and releases must preserve those notices and provide the corresponding source as required.
