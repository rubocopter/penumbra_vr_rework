# Releases

## v0.1.0-alpha.1 — 20 August 2026

Experimental public alpha. The full game is playable in VR, but this is not a
stable release: expect bugs and behaviours that still need polish. The
installer backs up every replaced file and can restore them, so trying the mod
does not risk the stock game.

Fixed installer compatibility on systems where the underlying .NET Framework
does not provide System.IO.Path.GetRelativePath (Windows PowerShell 5.1 /
legacy .NET Framework).

Fixed after the initial alpha download (included in this package):

- Recenter with the Create button now also works in right-handed mode: the
  movement input was silently overriding the recenter flag and dropping an
  idle Create press into the legacy fallback, which has no Create on PS VR2.
- Interaction ray uses the SteamVR aim pose on every controller profile, with
  automatic grip-pose fallback; a missing aim binding can no longer break hand
  tracking.
- Thrown and released objects inherit the real hand velocity rotated into
  world space, so throws go where the gesture points instead of whipping
  backwards.
- The death screen, examine/gameplay messages, and radio subtitles are
  anchored at the UI Distance setting instead of glued to the headset.
- Equipped HUD items no longer render collision-only submeshes; flashlight
  beam origin sits at the lens instead of floating ahead.
- Easier grabbing: larger hand interaction volume and a firmer held-object
  feel.

### What is in this build

- Device-independent SteamVR Input actions (gameplay, UI, and global action
  sets) with automatic fallback to the historical OpenVR polling path.
- A single rigid TrackingToWorld transform for the HMD, both hands, held
  objects, and world-space UI; orientation recentering; snap/smooth/disabled
  turning through world yaw.
- Persistent VR comfort settings in the `[VR]` section of `settings.cfg`,
  editable in-game under **Options → VR Settings**, grouped into CONTROLS,
  MOVEMENT, CALIBRATION, and DISPLAY sections, with contextual disabling
  (snap angle vs smooth speed depending on Turn Mode; "Detection depth"
  label under physical crouch) and a one-press Recenter action row.
- Dominant-hand selection (Left/Right) that reselects the mirrored action set
  and drives interaction, pointer, throw, and haptics.
- Seated and standing play modes; player height with a Calibrate button that
  measures the HMD height and rejects implausible samples.
- Physical, button, and hybrid crouch that drive the real gameplay collider;
  physical crouch calibrates a standing HMD height with a plausibility band
  (0.90–2.20 m).
- Room-anchored fullscreen menus and cinematics with scalable text;
  compositor-safe black loading frames for map changes, new game, tutorial,
  and saved-game routes.
- Controller-ray menu and inventory pointing, restored inventory action
  popups, and localized inventory actions.
- Semantic haptics routed through intent-named profiles.
- Room-scale collision reconciliation for swing doors (no walking through
  while leaning) and the held-door collision bypass fix.
- Rigged HUD hand models animated from the SteamVR skeletal summary.
- A reversible installer that merges the mod into the Steam installation,
  backs up replaced files, and restores them on request; double-click
  `Install-PenumbraVR.bat` installs, `-Restore` uninstalls.

### Known limitations

- **Experimental.** The hand fold state is frozen as an experimental final and
  still shows incorrect visual behaviours (ring/middle finger fused tube,
  limited index-finger range, basic grip/trigger blend poses).
- PS VR2 Sense is the only controller family validated on hardware. HTC Vive,
  Valve Index, Meta Quest/Oculus Touch, and Windows Mixed Reality bindings are
  bundled as default profiles but are untested on hardware. Native OpenXR is
  not implemented.
- Dominant-hand, play-mode, and player-height settings are implemented but
  have not had a full recorded hardware pass on every route.
- The build is Win32 (legacy third-party binaries); the engine rendering keeps
  the OpenVR compositor path.
- No guarantee of behaviour across graphics drivers or unusual Windows setups;
  controller reconnection and unusual SteamVR configurations receive limited
  testing.

### What to test

The full regression checklist is in the readme. The highest-value checks for
an external tester: Steam launch and tutorial, recenter, new game and save
loading, inventory use and combining, both handedness modes, the three turn
modes, the three crouch modes (physical/button/hybrid), seated play mode, and
the Calibrate button on the Player Height row.

### Reporting problems

Open an issue at <https://github.com/rubocopter/penumbra_vr_rework/issues>.
Attach the `hpl.log` from the game folder and, for installer problems, the
console output of `Install-PenumbraVR.ps1`.