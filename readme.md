# Penumbra: Overture VR Rework

Community-maintained continuation of [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr), the experimental VR port of Frictional Games' Penumbra: Overture.

This fork is unofficial and is not affiliated with Frictional Games, Valve, or Sony Interactive Entertainment. It does not include the commercial game. You must own Penumbra: Overture to use it.

## What this project is

Penumbra: Overture VR Rework is a modernization of the experimental VR port of
Penumbra: Overture originally published as [veryjos/penumbra_vr](https://github.com/veryjos/penumbra_vr).
That mod added head and hand tracking for the HTC Vive on top of the HPL1
engine. This rework rebuilds the port incrementally on a device-independent
SteamVR Input baseline: a single tracking-to-world boundary, semantic input
actions with a legacy OpenVR fallback, persistent VR comfort settings,
room-anchored UI, staged loading, and rigged animated hands. It is not a
rewrite: the engine remains Win32 because of its legacy binary dependencies and
rendering keeps the established OpenVR compositor path.

## Project status

The active development branch is `development`. It is an alpha/development
build: the game is playable, but it is not a stable release and may contain
bugs.

PS VR2 (SteamVR on PC) is the primary development platform. The bundled
bindings for other controllers (HTC Vive, Valve Index, Meta Quest/Oculus
Touch, Windows Mixed Reality) are included as default profiles but have not
been hardware-tested by this project.

Implemented so far:

- Device-independent SteamVR Input actions (gameplay, UI, and global action
  sets) with automatic fallback to the historical OpenVR polling path.
- A single rigid TrackingToWorld transform for the HMD, both hands, held
  objects, and world-space UI; orientation recentering; snap/smooth/disabled
  turning through world yaw.
- Persistent VR comfort settings in the `[VR]` section of `settings.cfg`,
  editable in-game under **Options → VR Settings** (dominant hand, seated or
  standing play mode, player height with a Calibrate button, movement speed
  and dead zone, height offset, turning, UI distance and scale, crouch mode
  and depth, subtitle scale).
- Physical, button, and hybrid crouch that drive the real gameplay collider.
  Button crouch lowers the rendered viewpoint by the configured crouch depth;
  physical crouch calibrates a standing HMD height with a plausibility band.
  Seated play mode applies a height offset at the tracking boundary instead
  of changing world scale, and player height is a separate semantic value
  with a Calibrate button that measures the HMD height.
- Room-anchored fullscreen menus and cinematics with scalable text and no
  opaque letterbox/subtitle bands; compositor-safe black loading frames for
  map changes, new game, tutorial, and saved-game routes.
- Controller-ray menu and inventory pointing (dominant hand, with fallback to
  the other hand when its pose is lost), restored inventory action popups, and
  localized inventory actions.
- Semantic haptics routed through intent-named profiles with per-event repeat
  limits.
- Room-scale collision reconciliation that keeps HPL1's native character
  collision while manipulating swing doors in VR, and fixes the held-door
  collision bypass of the original port.
- Rigged HUD hand models animated from the SteamVR skeletal summary (grip and
  trigger finger curls). This is the most recently worked-on area and remains
  experimental (see below).
- A reversible installer/deployer that merges the mod into the Steam
  installation, backs up replaced files, and can restore them.

The executable is Large Address Aware, and every automated build performs a
full solution rebuild to prevent ABI-stale objects in the legacy project.

Still experimental or not yet validated:

- **VR hands**: animation and tracking have received recent work and remain a
  likely area of change. The current fold state is frozen as an experimental
  final and still shows incorrect visual behaviours. Known limitations: the
  ring and middle fingers share a fused mesh tube, the index finger has a
  limited free tube, and the poses are basic grip/trigger blends.
- Controller profiles other than PS VR2 Sense are untested on hardware. Native
  OpenXR remains future work.

## Current state

As of the latest commits on `development` (August 2026):

- The full content of the commercial game remains playable in VR; the entry
  routes (tutorial, new game, continue, explicit save loading) were exercised
  on PS VR2 hardware on 14 August 2026.
- The hand rig work is in an "experimental final" state: bind matrices,
  weights, fold direction, and per-joint angles were reworked between 15 and
  18 August 2026 and are intentionally frozen for now, but this is the area
  most likely to change next and the current fold state still shows incorrect
  visual behaviours.
- Physical crouch now calibrates the standing HMD height during gameplay and
  rejects implausible samples (outside 0.90–2.20 m), fixing a wrong baseline
  that could be captured after loading a save; button crouch lowers the view
  by the configured crouch depth instead of the deeper original move-state
  offset. The crouch and height system has been revalidated and is confirmed
  correct.
- Dominant-hand, seated/standing play mode, and player-height settings were
  added on 18 August 2026. Handedness drives gameplay interaction, pointer,
  throw, and haptics without touching SteamVR bindings; seated mode applies a
  height offset at the tracking boundary instead of changing world scale; the
  player-height Calibrate button measures the HMD height. These settings are
  implemented but have not yet been validated on hardware.

## Validation

Hardware testing so far has been done on PS VR2 Sense controllers through
SteamVR on PC. Keep the following in mind:

- The 14 August 2026 milestone validated Steam launch, tutorial, new game,
  continue, saved-game loading, repeated inventory use, all crouch modes, menu
  transitions, controller reconnection, and locked-door collision on hardware.
- The hand rig was validated on hardware on 15 August 2026 (both hands render,
  fingers pivot at their joints); the bind-layout and weight fixes were also
  verified with an engine-exact simulation (`scripts/verify_bind_fix.py`).
- The crouch and height changes of 16–18 August 2026 have been revalidated on
  hardware and are confirmed correct. The hand tuning of 18 August 2026 has not
  had a full recorded pass after the retuning, and the fold state currently
  shows incorrect visual behaviours. A past validation is not a guarantee that
  the current build behaves identically. Other SteamVR devices still need
  hardware-specific testing.

Regression checklist for a future release (PS VR2, SteamVR):

1. Startup: Steam launch, the VR tutorial as the initial map, main menu visible
   in room space, recenter works.
2. New game, continue, and save/load routes complete without freezing and show
   the black loading frame.
3. Inventory: open, point, select an action, use an item on the world,
   drag/combine; popup text is readable.
4. VR hands: both hands render, fingers curl with grip and trigger, the thumb
   follows its hierarchy, no joint collapse at the wrist.
5. Recenter: horizontal orientation aligns in front of the view.
6. Crouch: physical, button, and hybrid modes enter and leave crouch; button
   crouch does not clip the camera; the standing height calibrates within the
   plausible band and recovers after a save load.
7. Map transitions: door and level changes hold the compositor fade and reveal
   the new map.
8. Controller disconnect/reconnect: a lost hand hides and releases held input,
   the other hand keeps pointing, and SteamVR reacquires the pose.
9. Doors: held swing doors keep their collision (no walking through while
   leaning), ordinary walls block movement.
10. Log checks: `hpl.log` reports the active input path (`SteamVR Input actions
    are active.` or `Falling back to legacy controller polling.`) and no
    repeated `[VR ...]` warnings.
11. New settings: dominant hand (Left/Right) changes which hand interacts,
    points, and receives haptics; seated play mode raises the world to the
    configured player height; the Calibrate button on the Player Height row
    writes the standing HMD height (outside 0.90–2.20 m it is rejected).

See [the development roadmap](docs/ROADMAP.md) for the planned stages and
[the VR architecture](docs/VR_ARCHITECTURE.md) for the tracking, world-space,
height, hand-state, and gameplay boundaries. The current boundary between
SteamVR, HPL1 input, and Penumbra gameplay is mapped in
[the input architecture](docs/INPUT_ARCHITECTURE.md), and [controller bindings
and VR settings](docs/INPUT.md) describe the complete current layout.

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

The base Spanish localization originated with the official Penumbra: Overture translation, was prepared for later Windows and Linux digital releases by DeM, and was revised by Oscar “darkpadawan” Rodríguez for [Clan DLAN](https://www.clandlan.net/). This project adds only the VR-specific strings. Redistribution of the full language file is confirmed by the project maintainer.

## Original project

The original mod added head and hand tracking for the HTC Vive. Its last upstream commit was published in 2018. Historical releases and preview material remain available through the links in the upstream repository.

## License

The HPL1 engine and Penumbra: Overture source in this repository are distributed under GNU GPL v3 or later; see `HPL1Engine/COPYING` and `PenumbraOverture/COPYING`. OALWrapper and bundled third-party components retain their respective notices and licenses. Modified source distributions and releases must preserve those notices and provide the corresponding source as required.
