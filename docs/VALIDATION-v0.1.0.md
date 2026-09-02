# v0.1.0 validation record

## Release decision

The user reports no problems after visiting multiple zones with the complete
231-image batch. The 2 September 2026 logs support promoting the accepted v4
renderer and this texture selection to the first stable baseline. They do not
establish full-campaign coverage, peak memory headroom or support for every
headset. No gameplay, renderer or asset changes are introduced for this release;
the executable's release identifier changes from `v0.1.0-beta.1` to `v0.1.0`.

## Tested installation and evidence

- Development source: `42040ab575266dda169a412643ebb34935325aa7`.
- Installed executable SHA-256:
  `A88F605CE01D5E1F053B2F8450E7EFA77F8C6E50622303AC2D6736C2694DBC71`.
- All **231 installed selected images** match `TEXTURE_SELECTION.json` exactly.
  Installation state records deployment on 2 September at 07:15 local time.
- Hardware confirmed by the user: **PS VR2 and Sense controllers**. SteamVR's
  server log independently records the PlayStation driver activating that HMD
  and both Sense devices. HPL sees `oculus` / `oculus_touch` through the client
  properties in this session; the cause of that identity discrepancy was not
  isolated. Do not count this as physical Quest/Touch validation.
- RTX 4070 Ti, OpenGL 4.6 / NVIDIA 616.56, SteamVR 2.17.8, 90 Hz target.
- Eye buffers **3400×3468**, render scale **1.00**, Enhanced visuals **On**.
  Both auxiliary float/MSAA targets and the enhanced shaders initialized.
- HPL log SHA-256:
  `B077A4F6E19156604058DF1F796E372FED2EE263A9223424ECA58C829EB0F054`.
- Compositor log SHA-256:
  `EBF79B155F7DF62E592E3D25978B7CB5511A902A7394C2223B64122DB72A7F5A`.

Raw logs are retained locally under `build/release-audit-2026-09-02`, excluded
from Git and the public package. They include machine-specific paths and device
details; this record contains the release-relevant results only.

## Coverage and shutdown

The compositor records the game connection from approximately 07:19:37 to
07:36:43 local time: **17 minutes 6 seconds**. HPL records eight completed map
loads across six distinct maps:

| Map | Loads | Logged load time |
| --- | ---: | ---: |
| `level01_03_shafts` | 3 | 4163 / 2686 / 2657 ms |
| `level00_00_vr_tutorial` | 1 | 1941 ms |
| `level00_01_boat_cabin` | 1 | 1675 ms |
| `level01_01_outside` | 1 | 1199 ms |
| `level01_02_mine_entrance` | 1 | 1474 ms |
| `level01_04_old_storage` | 1 | 1904 ms |

These are load durations in this run, not comparisons against original textures.
The user's broader positive feedback is recorded separately from the exact map
coverage present in this log. The log contains held-light and mechanism use,
but it does not substitute for matched screenshots of every lighting condition.

Exit reaches `HPL Exit was successful` and the HPL memory manager reports no
remaining allocations. No fatal error, out-of-memory report, new shader/FBO
failure or Windows application-crash event for the game was found in the
reviewed session interval. The existing crash dump predates this run (28 August).
Engine leak reporting does not measure driver allocations or prove peak-memory
headroom.

## Performance

The compositor's **game PID 1432** cumulative block reports:

- 92006 presents, 986 reprojected: **1.072%** relative to presents.
- 66 dropped: **0.072%** relative to presents; 33 are in its startup category.
- ApplicationTime CPU **4.640 ms**, GPU **1.744 ms**; compositor CPU **0.209 ms**,
  GPU **0.334 ms**, with a 90 Hz target.

The separate global `Total dropped frames: 1337` is not the game's PID-specific
drop count. Loading, startup and dashboard intervals are not a controlled
gameplay benchmark; these session averages cannot establish worst-case pacing.

HPL reports **707 windows of 120 samples for each eye**, all On. The following
are means of those equally sized windows, not a single-frame percentile:

| GPU elapsed query | Left | Right |
| --- | ---: | ---: |
| Scene/UI mean | 1.387 ms | 1.291 ms |
| MSAA colour resolve mean | 0.245 ms | 1.045 ms |
| Final image mean | 0.219 ms | 0.183 ms |
| Sum mean | **1.851 ms** | **2.520 ms** |
| 95th percentile of window sums | 2.967 ms | 3.962 ms |
| Maximum window sum | 5.240 ms | 5.344 ms |

The right-eye resolve is measurably higher and variable. Source inspection
confirms that each query surrounds that eye's resolve/final stages, finishes
before compositor submission and does not nest or synchronously wait for a
busy query. The logs do not isolate driver scheduling, synchronization or
compositor contention as its cause. Keep this as a profiling follow-up; do not
label it a texture regression or silently discard the larger measurements.

GPU elapsed-query sums and SteamVR ApplicationTime use different boundaries and
must not be added together or treated as interchangeable. There are **no Off
windows** and no original-texture baseline here, so this run cannot quantify the
incremental cost of Enhanced visuals or the texture expansion.

## Recoverable warnings

The log is not warning-free. None of its warnings/errors names any of the 231
selected image files. There are 20 `ERROR:` lines concerning sound, material,
mesh, particle or script/entity resources, plus repeated import warnings:

- Missing `unbreak_impact_fence_med` / `unbreak_impact_snow_med` sound entities
  are referenced by the unchanged original `materials.cfg`.
- Missing `lambert1-fx`, `blinn7-fx`, `text_look-fx` and
  `oldmachine_lambert5-fx` names are legacy material/effect references, not the
  replacement JPEG names. Mesh bone/joint and light-parent attachment warnings
  accompany legacy content loading, including the existing VR tutorial.
- Empty material names, missing particle definitions, a `NAME_item.ent`
  placeholder, and missing ladder/lock entities or timers remain content/script
  follow-ups. They are not proven harmless in every circumstance; the reported
  session continued through them without a user-observed problem. No definitions
  or scripts were altered in the texture expansion.
- 67 automatic and two manual height-calibration rejections occur initially
  while the reported HMD height is around 2.5 m. Calibration later succeeds,
  first at 0.96 m and subsequently at 1.54 m. The plausibility guard is working;
  the initial tracking-floor cause was not established.
- Both hands lose their action poses around 95 seconds, then recover around
  97 seconds after 46 invalid-pose frames per hand. A startup skeletal-summary
  error is also logged. Neither becomes a persistent missing-hand condition.
- SteamVR reports SRV-creation errors during compositor startup, before the
  game connects. These are runtime-side observations, not HPL shader failures.

None of these findings warrants silently changing the tested renderer or
installing additional content as part of the stable-version promotion.

## Memory and compatibility limits

The log's 3953 MiB free process-address-space figure is from **startup**, before
maps and their texture caches are resident. There is no gameplay peak process
commit, free-address-space trace or VRAM measurement. The 608.875 MiB incremental
texture estimate remains storage arithmetic, not a measured allocation peak.
The app is still Win32 / Large Address Aware. Keep the texture-only rollback and
do not infer unlimited campaign-cache headroom from this successful session.

PS VR2 Sense is the only physically validated controller family. Full campaign
mechanisms, other GPUs/drivers/headsets and true fallback hardware still require
reports. The optional visual path remains Off by default, with the original
renderer/failure fallback retained. Installed textures affect On, Off and the
monitor until removed using `Install-PenumbraVR.bat -SkipTexturePack`.

## Automated checks

The tested development checkpoint passed both Windows CI workflows, a local
Release Win32 package, 289 engine/VR checks, 16 bundled-Cg shader compilations,
8752 CPU visual-reference checks and all 231 legacy SDL_image decodes. Texture
deployment/upgrade/rollback passed 944 assertions on PowerShell 7 and Windows
PowerShell 5.1. Source attribution, policy, per-image hashes and the global
texture budget are checked by the packaging workflow.

The stable version is rebuilt and packaged after the identifier/documentation
update. Release assets must pass those build checks and manifest verification;
the final executable is not expected to retain the development executable hash
above because its version/build stamp changes.
