# Release texture selection

## Stable batch — 2 September 2026

**231 opaque diffuse replacements** are included in `v0.1.0` (they were not
part of `v0.1.0-beta.1`). This is one reviewed batch: 130 images for
models and 101 for scenery, covering doors, cupboards, desks, shelves, machinery,
brickwork, metal, wood, caves and other surfaces. The nine previously tested
images remain byte-identical. The other 222 are resized conventionally and
encoded as high-quality JPEG, as requested by the user. No generative edits,
additional sharpening, material edits or renderer changes were made.

The office now includes `office_cabinet`, `office_drawermat`,
`mining_room_bookshelf` and `mining_room_desk`. The broad audit confirmed from
HPL's `MaterialManager::GetAnimMode()` that an empty `AnimMode` means static,
which admitted legacy office materials rejected by the first strict filter.

This is a screened selection, **not a guarantee that no map can regress**.
The user reports no problems after a grouped PS VR2 test; the inspected session
covers eight loads across six maps and a clean shutdown. All 231 installed
images match the manifest, with no selected-image warning in the log. Peak
memory and matched original-texture timing remain unmeasured. See the
[validation record](VALIDATION-v0.1.0.md). The accepted v4 lighting, flashlight
alignment and VR options layout are unchanged.

## Source and review

The supplied folder, `Desktop/proyecto_penumbravr/reemplazo_texturas`, contains
1014 files and approximately 2.45 GiB of data. The user identified it as
**Penumbra Collection 4x AI upscale** by **Bret2011**, uploaded by
**c3pohumancyborgrelations**, crediting **Frictional Games**. See
[attribution and permissions](TEXTURE_CREDITS.md).

Every supplied file was inventoried. The final material/format/name audit found
280 potential opaque diffuse replacements; 231 survived the resolution policy
and visual review. Side-by-side sheets covered all 280 candidates, supplemented
by downsampled colour/correlation checks and inspection of representative final
outputs. These checks establish composition/colour consistency, not guaranteed
perceptual improvement, correct appearance on every UV seam or freedom from VR
shimmer.

- [TEXTURE_AUDIT.json](TEXTURE_AUDIT.json) records inclusion/exclusion for all
  1014 files, including files that were never candidates.
- [TEXTURE_SELECTION.json](TEXTURE_SELECTION.json) records every included path,
  source/output/original hash, dimensions, material references and memory cost.
- [TEXTURE_POLICY.json](TEXTURE_POLICY.json) fixes the resolution, compression,
  budget, original nine-image exception and explicit visual exclusions.

The audit checks every indexed material use, not just the filename. A candidate
must remain RGB24, opaque, static, diffuse-only, depth-tested and mipmapped, with
the same path, aspect ratio and power-of-two dimensions. Resource basenames must
be unique across installed image formats/directories because HPL resolves names
globally. Actual HUD/player mesh image references and existing mod-owned images
are protected. Originals already replaced by this mod are read from the
installer's hash-verified backups, not from installed replacement files.

Excluded classes include normal/specular/emissive maps, transparencies, decals,
animated images, special material uses, hand/HUD/held-item assets, definitions
and meshes, ambiguous names and unused/unresolved resources. Navigation maps,
keypad digits, instrument scales, notes, diagrams and suspect printed artwork
remain original. Decorative labels on otherwise accepted props may change.
Uniform grey, the changed rope/metal patterns and conspicuous new powder specks
were rejected. Thirty otherwise eligible images already reach the dimension
cap at stock resolution, so there is no resolution gain worth importing them.
Exclusion does not necessarily mean an asset is incompatible; it can mean that
its benefit is uncertain or it falls outside this low-risk scope.

## Resolution, compression and memory

New entries use **2× stock dimensions, capped at 1024 on the longest side**,
with aspect ratio preserved, high-quality bicubic reduction and JPEG quality
95. The nine existing images keep their original pack bytes and sizes (up to
1024). No selected entry is lower-resolution than stock.

| Measurement for the same 231 images | Unmodified pack files | Selected outputs |
| --- | ---: | ---: |
| Disk size | 251.67 MiB | 59.23 MiB |
| Conservative RGBA + complete mip chains | 3087.33 MiB | 801.83 MiB |
| Increment over stock RGBA + mips | 2894.38 MiB | 608.875 MiB |

The enforced **global incremental budget is 640 MiB**. This deliberately sums
all selected textures, not only one map. `MapHandler` loads the next world
before destroying the previous one, and its world cache retains resource
references, so a one-room estimate would miss transition overlap.

JPEG compression reduces disk/download size, **not the decoded GPU texture
size**. Dimension reduction supplies the memory saving. The original materials'
compression settings are untouched: this is not a DDS/BC texture-pipeline
conversion. Full mip chains remain enabled by the original materials.

These figures are storage arithmetic, not measured process commit, peak load
memory or VRAM. Decoder temporaries, driver copies, fragmentation, caches and
the existing VR eye buffers add overhead. There are no additional texture
fetches, but larger textures can still affect loading, cache traffic and GPU
time. Large Address Aware allows up to 4 GiB of user virtual address space on
64-bit Windows; it does not remove these limits or prove runtime headroom.
See [Microsoft's memory limits](https://learn.microsoft.com/en-us/windows/win32/memory/memory-limits-for-windows-releases).

## Install once, compare as a group

Close the game, then run the packaged `Install-PenumbraVR.bat`. Existing originals
are backed up; upgrading from the previous nine-image selection preserves their
original backups. **Enhanced visuals Off does not remove textures**: both On,
Off and the monitor see installed assets. The lighting toggle remains live;
changing the texture selection requires closing/restarting the game.

- Include/re-enable the whole selection: `Install-PenumbraVR.bat`.
- Restore only these images and retain the VR mod:
  `Install-PenumbraVR.bat -SkipTexturePack`.
- Restore the pre-mod installation: `Install-PenumbraVR.bat -Restore`.

One useful grouped session: inspect office desks/shelves/cupboards with and
without the flashlight, then brick/metal surfaces and a cave; move the head to
check shimmer and cross map boundaries to exercise loading/caching. Keep render
scale 1.00 and the accepted lighting calibration. Inspect logs and process-memory
headroom afterwards. There is no need to reinstall individual textures or run
a separate experiment for each image. If the batch regresses, the single
texture-only rollback restores the original asset baseline. The installer
refuses to overwrite externally modified managed files during rollback.

## Reproduce and validate

- `audit-texture-pack.ps1 -SourceRoot ... -GameRoot ...` inventories the complete
  folder and checks material/resource/VR conflicts without modifying the game.
- `review-texture-candidates.ps1 -SourceRoot ... -GameRoot ...` creates diagnostic
  comparison sheets and source/original hash-bound colour/alignment metrics in
  `build/texture-audit`. Inspect those sheets before importing.
- `import-texture-selection.ps1 -SourceRoot ... -GameRoot ... -UseCurrentAudit`
  applies the reviewed policy to those exact inputs, stages the entire batch,
  validates it, then copies only selected images into `data`. Without that switch
  it regenerates audit/review data and stops for inspection. It rejects stale
  image reviews and unowned/externally modified project destinations.
- `check-texture-selection.ps1` checks hashes, formats, policy/audit agreement,
  the budget and absence of extra JPEGs before build and after packaging.
- `check-texture-decoder.ps1` loads **all 231 images with bundled x86 SDL_image**,
  verifying RGB24 and exact dimensions. It runs before every build without a
  game, OpenGL context or headset.
- `check-texture-processing.ps1 -SourceRoot ...` compared sampled pixels of all
  222 encoded outputs to their uncompressed resized references: minimum PSNR
  **37.065 dB**, maximum mean RGB error **2.3163/255**. These are compression
  checks, not a guarantee of invisible loss or better graphics.
- Installer fixtures passed **944 assertions** on PowerShell 7 and Windows
  PowerShell 5.1: old nine-image install/skip, upgrade, texture-only restore,
  re-enable and full restore. Tests use disposable fixtures, never the real game.
- Final Release Win32 build/package passed project validation, 16 Cg shader
  compilations, 8752 visual-reference checks and 289 engine/VR tests. All 309
  packaged file hashes were verified, including another legacy-decoder pass on
  the packaged images. The executable SHA-256 remains the tested v4
  `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71`.
- Reimporting the selection reproduced the identical manifest and image hashes.
  Read-only checks confirmed that the real installation still contained the
  previous nine-image selection and the 222 unchanged originals before handoff.

The source pack and actual installed game are not modified by the audit/import
workflow. The first nine-image trial (37.5 MiB incremental estimate) remains in
the lighting/release history; the current expansion supersedes its scope, not
its original-file backups or credits.
