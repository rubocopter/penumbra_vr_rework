# Development texture selection

These nine textures are included in development packages, not in the published
`v0.1.0-beta.1`. Further expansion remains a separate scope decision; the latest
positive graphics feedback is not an exhaustive texture-validation result.

The supplied folder was located as `Desktop/proyecto_penumbravr/reemplazo_texturas`.
It contains 1014 files, about 2.45 GiB on disk, including HUD definitions,
models, normal maps and uncompressed 4096 textures. It is **not** overlaid wholesale.
The user subsequently identified it as **Penumbra Collection 4x AI upscale** by
**Bret2011**, uploaded by **c3pohumancyborgrelations**, with original asset credit
to **Frictional Games**. The source page and redistribution permissions were
verified; see [attribution and permissions](TEXTURE_CREDITS.md). The initial
unknown-provenance notice is superseded. No texture was edited or recompressed.

## Selected resources

Nine static opaque diffuse JPEGs were compared to the installed originals.
This is a conservative first selection, **not every VR-compatible texture**.
The initial filter yielded 55 candidates inside its restricted directory/1024
budget rules; nine were selected after review. That filter excluded 882 files by
scope/name/type, 75 by size and two by ambiguous resource name. Those exclusions
do not prove incompatibility: ordinary prop textures and larger world textures
need separate review and a memory budget. UI/cubemap issues mentioned by the
pack author are distinct from actual conflicts with the VR hand/HUD assets.
For example, the pack has `office_cabinet`, `mining_room_bookshelf` and
`mining_room_desk` diffuse JPEGs at 2048² versus the originals' 512². None has
an identical-path collision with current mod assets, and their inspected
materials are opaque. They nevertheless fall outside the original cap and add
about 20 MiB each (RGBA+mips); full resource-name, visual/UV and memory review
is still needed before selection. They have **not** been imported.
The nine selected images keep the same composition, aspect ratio and resource
path. Each selected resource
basename is unique in that installation. Material references were parsed and
required to use the image only as non-animated 2D diffuse, with mipmaps,
depth testing and no alpha. Original materials, normal/specular maps, UVs and
sampling settings are retained. Visual review is not a map-wide guarantee.

| Directory | Images | Replacement size |
| --- | --- | --- |
| iron_mine | iron_mine_briks, iron_mine_pillar | 1024² |
| iron_mine | iron_mine_pipe_end | 512² |
| locker_room | locker_room_walltiles, locker_room_walltiles_dirty | 1024² |
| modern_mine | modern_mine_green_metal, modern_mine_pipe | 1024² |
| modern_mine | modern_mine_bluemetal | 512² |
| new_storage_room | new_storage_room_rust | 1024² |

All filenames end in `.jpg`. Their exact hashes, original hashes, dimensions,
reviewed materials and estimates are in [TEXTURE_SELECTION.json](TEXTURE_SELECTION.json).
Some are reused by the VR tutorial's world geometry; no hand, controller or
held-item asset is replaced. Map coverage depends on the actual resources used
in the scene: this is not a whole-game texture overhaul.

Excluded: all `models`, HUD/hand/flashlight/glowstick assets; `.hud`, `.bnt`,
`.beam`, material definitions, normals/specular/emissive maps, alpha/transparency,
animated frames, signs/puzzle information, decals, ambiguous names, dimensions
above 1024 and nonmatching aspect ratios. `mine_rustymetal.jpg` was dropped
because its original material enables alpha. Near-uniform images and upscales
with doubtful visual benefit were also left out; resolution alone is not merit.

## Memory and LAA

Selected JPEG disk total: **4,016,328 bytes (3.83 MiB)**. A conservative
uncompressed RGBA + full mip-chain estimate is **40 MiB total**, **37.5 MiB
additional** over the original images, if all nine are resident together.
The importer imposes a 40 MiB *incremental* budget. This is texture storage
arithmetic, not measured VRAM, process commit or peak loading memory: driver
copies, image decoding, fragmentation and the existing eye buffers also matter.
There are no additional texture fetches from increasing these resolutions.

Large Address Aware lets this 32-bit executable address up to **4 GiB of user
virtual address space on 64-bit Windows**, versus the normal 2 GiB. It neither
converts the engine to 64-bit nor grants unlimited texture memory. See
[Microsoft's memory limits](https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases).
That extra headroom does not justify loading the entire multi-gigabyte pack.
The full game plus VR targets still needs runtime observation over map changes.

## Install and compare

The normal installer includes the nine files and backs up their previous
versions using the existing deployment manifest. **Enhanced visuals Off does
not undo asset replacements**: it selects the original renderer using whichever
textures are installed. The monitor likewise sees the installed textures.

- Include/re-enable: `Install-PenumbraVR.bat`.
- Omit or restore only the texture selection: `Install-PenumbraVR.bat -SkipTexturePack`.
- Restore the entire pre-mod installation: `Install-PenumbraVR.bat -Restore`.

Close the game before installing/restoring, then restart to reload textures.
Renderer On/Off still changes live without reloading. Do not copy the full
supplied folder into the game. Existing unrelated resources remain untouched.
The installer refuses to replace externally modified stale files during restore.

## Reproduce and verify

`scripts/audit-texture-pack.ps1` inventories candidates without modifying the
game or pack. `scripts/import-texture-selection.ps1 -SourceRoot ... -GameRoot ...`
imports the fixed reviewed allowlist after rechecking metadata, unique names
and every material use; it refuses to overwrite differing project assets.
The JSON manifest is generated from that import. `check-texture-selection.ps1`
verifies hashes, formats, dimensions, budget and absence of extra selected-folder
JPEGs before building and after packaging. No installed game is needed for those
build checks. Source and installed game files were not changed by the import.

`scripts/test-texture-deploy.ps1` passed 38 assertions on PowerShell 7 and
Windows PowerShell 5.1: install, selective restore, re-enable, full restore and
retention/removal of the mod executable as appropriate, all in disposable
fixtures. At the initial import/fixture-validation checkpoint, the real
installed texture hashes still matched the original baseline; this is not a
claim about the user's installation after later tests. Release Win32 full build
and package checks passed.

Pending hardware checks: compare close-up brick/metal detail and moving-head
shimmer, verify normal-map alignment and transitions, revisit several maps,
observe peak process memory and per-eye GPU timings. No claim of zero risk or
zero performance cost is made before that test.
