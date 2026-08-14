# Penumbra: Overture VR Rework

Community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the experimental VR port of Frictional Games' Penumbra: Overture.

This fork is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment. It does not include the commercial game. You must own Penumbra: Overture to use it.

## Project status

The `modernization` branch provides a working SteamVR baseline built around
OpenVR 2.15.6 and device-independent SteamVR Input actions. It includes a
native PS VR2 Sense profile, preserves the historical HTC Vive profile, and
falls back to legacy OpenVR polling when action input is unavailable. Rendering
continues to use the established OpenVR compositor path, and the engine remains
Win32 because of its legacy binary dependencies.

The current milestone adds configurable snap/smooth turning and locomotion,
physical/button/hybrid crouch with matching camera height, orientation
recentering, room-anchored fullscreen UI, scalable text, controller-ray menu
and inventory input, localized inventory actions, semantic haptics,
compositor-safe loading transitions, and room-scale collision correction. It
also fixes held-door collision bypasses and several teardown/input lifecycle
problems inherited from the experimental port. The executable is Large Address
Aware and every automated build performs a full solution rebuild to prevent
ABI-stale objects in the legacy project.

Steam launch, tutorial, new game, continue, saved-game loading, repeated
inventory use, all crouch modes, menu transitions, controller reconnection, and
locked-door collision were validated on PS VR2 hardware on 14 August 2026.
Other SteamVR devices still need hardware-specific testing. Native OpenXR and
additional controller profiles remain later milestones. See [controller
bindings and VR settings](docs/INPUT.md) for the complete current layout.

See [the modernization roadmap](docs/ROADMAP.md) for the planned stages and
[the VR architecture](docs/VR_ARCHITECTURE.md) for the tracking, world-space,
height, hand-state, and gameplay boundaries. The current boundary between
SteamVR, HPL1 input, and Penumbra gameplay is mapped in
[the input architecture](docs/INPUT_ARCHITECTURE.md).

## Install a packaged build

1. Install Penumbra: Overture through Steam. If an older mod or installer has
   changed that copy, first use Steam's **Verify integrity of game files**.
2. Extract the complete `PenumbraVR` package to a normal writable folder.
3. Open PowerShell in the extracted folder and run:

   ```powershell
   .\Install-PenumbraVR.ps1
   ```

   If Windows blocks local scripts, use:

   ```powershell
   powershell -ExecutionPolicy Bypass -File .\Install-PenumbraVR.ps1
   ```

4. Install and configure SteamVR. PS VR2 users also need Sony's PlayStation VR2
   App and the required PC hardware.
5. Install the Microsoft Visual C++ 2015–2022 Redistributable (x86) if it is not
   already present.
6. Start SteamVR and launch **Penumbra: Overture** with Steam's normal **Play**
   button.

The installer detects Steam libraries and merges the mod directly into the
game's `redist` directory. It backs up replaced files and records every
mod-owned path under `.penumbravr` in the game root. A later installation
removes retired mod files, preventing stale DLLs or data from surviving an
upgrade. It replaces `redist/Penumbra.exe` with the VR build so Steam retains
launch and play-time tracking.

For a non-default installation that cannot be detected automatically, pass the
game root or its `redist` directory explicitly:

```powershell
.\Install-PenumbraVR.ps1 -InstallRoot 'X:\SteamLibrary\steamapps\common\Penumbra Overture'
```

To restore every backed-up original and remove files installed by the mod, run
the installer from the extracted package again with:

```powershell
.\Install-PenumbraVR.ps1 -Restore
```

Do not install from `build\bin\Release`: that directory contains only native
build output. Use the complete package so the executable, OpenVR loader,
bindings, localized data, maps, models, documentation, and checksums stay in
sync. The package never includes the commercial base game.

## Build from source

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

`-Deploy` uses the same reversible installer flow. It records every mod-owned path under
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

The build remains Win32 intentionally because the checked-in Newton, FLTK,
SDL, OpenAL, Cg, and other third-party libraries are 32-bit binaries. The
executable is linked with Large Address Aware, so on 64-bit Windows it can use
a larger user-mode virtual address space than a conventional 32-bit process;
this improves memory headroom but does not by itself make high-resolution
assets free of engine or GPU limits.

The build script deliberately invokes a full `Rebuild`, not an incremental
build. The legacy Visual Studio dependency metadata can otherwise retain
objects compiled against older class layouts and produce heap corruption.
Build products are written under `build/`; source directories are not modified.

The package is created at:

```text
build/package/Release/PenumbraVR
```

### Deploy a development build

1. Install Penumbra: Overture through Steam.
2. If another installer has modified the Steam copy, verify the game files first.
3. Run `./scripts/build.ps1 -Configuration Release -Deploy` from a source checkout.

Manual copying is discouraged because it cannot remove stale mod files or
restore originals automatically. If it is unavoidable, the package's
`config`, `maps`, and `models` folders merge directly under `redist`; never
create a `redist/data` directory.

### Spanish language

The package includes `config/Espanol.lang`. Select **Español** under **Options → Game → Language**. The VR tutorial, VR menu options, and controller notes have been translated for this rework.

The base Spanish localization originated with the official Penumbra: Overture translation, was prepared for later Windows and Linux digital releases by DeM, and was revised by Oscar “darkpadawan” Rodríguez for [Clan DLAN](https://www.clandlan.net/). This project adds only the VR-specific strings. The accompanying translation readme did not state explicit redistribution terms, so permission should be confirmed before publishing a formal release containing the full language file.

## Original project

The original mod added head and hand tracking for the HTC Vive. Its last upstream commit was published in 2018. Historical releases and preview material remain available through the links in the upstream repository.

## License

The HPL1 engine and Penumbra: Overture source in this repository are distributed under GNU GPL v3 or later; see `HPL1Engine/COPYING` and `PenumbraOverture/COPYING`. OALWrapper and bundled third-party components retain their respective notices and licenses. Modified source distributions and releases must preserve those notices and provide the corresponding source as required.
