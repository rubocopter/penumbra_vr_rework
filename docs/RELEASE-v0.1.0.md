# Penumbra: Overture VR Rework v0.1.0 — first stable

The first stable baseline of this community PCVR mod, following PS VR2 Sense
testing across multiple maps. Requires an owned copy of **Penumbra: Overture**;
the original game is not included. Windows 10/11 and SteamVR are required.

## Highlights

- Room-scale VR, collision-constrained motion-controlled hands, physical props
  and mechanisms, assisted inventory pickup, seated/standing and left/right-hand
  modes, and adjustable comfort controls.
- Binaural HRTF, wall/door occlusion, distance absorption and restrained ambient
  reverb through bundled OpenAL Soft.
- **Optional Enhanced visuals**, Off by default: MSAA 2×, refined ambient and
  surface lighting, dark image treatment and bounded sharpening. The accepted
  v4 calibration retains restrained flashlight/glowstick brightness and a
  smooth glowstick halo. Switch live in Options → VR Settings → Display.
- Correct flashlight beam/flare alignment and a two-column VR settings layout
  that keeps all controls reachable.
- **231 reviewed diffuse textures** covering scenery and props. The original
  nine stay unchanged; 222 additions use conventional bicubic reduction, a 1024
  maximum dimension and JPEG quality 95. The selection occupies 59.23 MiB on
  disk, with protected VR/held-item assets and special materials excluded.
- Reversible installation, texture-only rollback, app-local OpenVR/OpenAL/VC++
  runtimes and Large Address Aware Win32 support.

## Validation and limits

The grouped PS VR2 Sense session completed eight loads across six distinct maps
and exited cleanly, with no HPL-reported leaks or selected-image errors. At
3400×3468 per eye, scale 1.00, Enhanced visuals On and a 90 Hz target, the
roughly 17-minute session recorded **1.07% reprojection** and **0.07% dropped
frames** in the game's compositor statistics.

The release checks include Release Win32/Large Address Aware, 289 engine/VR
tests, 16 Cg shader compilations, 8752 visual-reference checks, all 231 images
decoded by the legacy x86 loader, and 944 installer upgrade/rollback assertions.

Stable does **not** mean every map, GPU or headset has been exhaustively tested:

- PS VR2 Sense is the only controller family validated on physical hardware.
  Index, Touch, Pico, WMR and Vive profiles are included but need device reports.
- The hand collider is an approximate palm volume, not per-finger physics.
  Thumb/ring/middle rig limitations and one active grab at a time remain.
- Recoverable legacy content/script warnings remain. Initial tracking/height
  calibration warnings recovered in this run; no new rendering failure was found.
- Right-eye resolve timing is higher/variable and remains a profiling item.
  This session is not a matched On/Off benchmark or a texture-cost comparison.
- Peak campaign memory use is not measured. The app remains 32-bit; the texture
  selection's 608.875 MiB incremental estimate is arithmetic, not measured RAM
  or VRAM consumption. Restore the original textures if memory pressure occurs.

The original renderer remains available with Enhanced visuals Off, with a
fallback when the enhanced targets/shaders are unsupported. There is **no SSAO,
ray tracing or HDR headset output**. Installed textures affect both visual modes
and the monitor until explicitly removed.

## Installation and rollback

1. Download `PenumbraVR-v0.1.0-Windows-Win32.zip` below and extract it completely.
2. Close the game and run `Install-PenumbraVR.bat`. It detects the Steam game and
   preserves backups when upgrading an existing mod installation.
3. Start SteamVR and launch Penumbra: Overture normally from Steam.

To omit/remove only the texture selection, run
`Install-PenumbraVR.bat -SkipTexturePack`. To restore the pre-mod installation,
run `Install-PenumbraVR.bat -Restore`. The installer protects externally modified
managed files instead of silently overwriting them during rollback.

The ZIP contains `SHA256SUMS.txt` for its files; the separate `.zip.sha256`
download verifies the complete archive.

## Texture credits

**Penumbra Collection 4x AI upscale**, created by **Bret2011**, uploaded by
**c3pohumancyborgrelations**, with original assets credited to **Frictional Games**.
Redistributed with creator credit under the author's upload/modification
permissions. The 222 new derivatives were resized and compressed conventionally;
no additional AI processing was used. These assets are not relicensed as GPL.

[Source pack](https://www.nexusmods.com/penumbraoverture/mods/5?tab=description) ·
[Credits and permissions](https://github.com/rubocopter/penumbra_vr_rework/blob/v0.1.0/docs/TEXTURE_CREDITS.md) ·
[Validation record](https://github.com/rubocopter/penumbra_vr_rework/blob/v0.1.0/docs/VALIDATION-v0.1.0.md) ·
[Controls](https://github.com/rubocopter/penumbra_vr_rework/blob/v0.1.0/docs/INPUT.md) ·
[Changes since beta.1](https://github.com/rubocopter/penumbra_vr_rework/compare/v0.1.0-beta.1...v0.1.0)
