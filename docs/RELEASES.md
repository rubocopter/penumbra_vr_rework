# Release history

## Unreleased

### Changed

- Reflowed Options → VR Settings into two columns so every setting and the Back button remains inside the 800×600 VR cursor range

### Documentation

- Added a rendering and lighting decision record covering the current HPL1 ambient/direct-light order, packed eye depth/stencil attachment, discarded half-resolution SSAO prototype, measured per-eye cost, visual findings, complete cleanup, and the minimal contract for a future VR-only hemispherical-ambient A/B
- Corrected the controls guide to reflect the current collision-constrained visible palm instead of the obsolete pass-through description

### Validation

- Release Win32 full rebuild, Large Address Aware verification, `289 checks, 0 failures`, package generation, executable hash comparison, and confirmation that runtime code, shaders and packaged resources contain no leftover SSAO implementation

---

## v0.1.0-beta.1 — 28 August 2026

**First public beta: full-size physical hands that slide and recover without opening a path through walls, plus a clean and deterministic audio shutdown.**

### Added

- A centralized VR hand-collision policy for the physical palm dimensions (`190 × 52 × 125 mm`), 20 mm spatial sweep step, contact skins, translation/rotation refinement, overlap resolution, tracking sanity limits, and constrained-hand recovery thresholds. Normal contact uses only a 2 mm numerical skin; a nearby physical interaction target may use up to 8 mm, still far below the palm's 52 mm blocking span
- Context-aware soft contact for nearby physical targets. It improves approach to drawer fronts, handles, doors, and props without shrinking the physical collider, skipping the target body, weakening magnetic-item occlusion, or changing collision elsewhere
- A body-side recovery path for a hand that starts overlapped after loading, re-enters after a large tracking discontinuity, or remains constrained in a concave hinge/ceiling contact while the controller is pulled back. Every candidate anchor must be collision-free and the complete palm is swept from that anchor to the controller; recovery cannot teleport through a wall or closed door
- Regression coverage for full palm dimensions, sustained wall pressure, a high-speed controller crossing a 40 mm closed door, interaction contact skin, frame-rate independence, motion-direction symmetry, rotation against a surface, initial ceiling overlap, closed-door recovery, and hinge recovery that activates only on pullback

### Fixed

- Hands no longer use the controller's final rotation for every translation sample. Translation and rotation are swept independently, so a small wrist turn at contact cannot create a deep artificial overlap or abruptly expel the hand
- Collision resolution now selects the contact normal most opposed to motion, refines the first blocking sample, and removes only the into-surface component. Hands stop close to barriers and slide along faces instead of being attracted to corners or frames by an aggregate diagonal correction
- Invalid or implausible tracking matrices are rejected before they reach the hand mesh, equipped item, attached light, or physics world. A bad sample holds the previous safe pose independently for each hand instead of sending geometry or lighting to arbitrary world coordinates
- All consumers of one OpenVR hand sample share the same resolved pose. Rendering, equipment, interaction, and crosshair updates no longer repeat collision resolution or observe different transforms within one frame
- Interaction may inspect only a tightly bounded amount of raw controller intent beyond a blocked palm, and only with solid-body line of sight. This keeps small handles reachable while walls and closed doors remain authoritative for both hands and either dominant-hand setting
- The exit-time `0xC0000374` heap corruption is fixed. Per-source EFX filters are now removed through the EFX manager that registered them, eliminating a double free when the source manager shuts down before the EFX manager
- EFX effects, slots, sources, and manager threads now initialize mutex/thread state deterministically, tolerate creation failure, stop workers before destroying synchronization, and null destroyed pointers. This also removes the earlier uninitialized-mutex access violation when legacy audio threading is disabled

### Validation

- Release Win32 full rebuild, Large Address Aware verification, project validation, and `289 checks, 0 failures`
- PS VR2 Sense hardware session covering walls, closed doors, lockers, drawers, physical props, both hands, crouch/recenter, recovery activations, and normal shutdown at an average 88.16 FPS
- The validated session ended with `HPL Exit was successful`, zero reported memory leaks, no invalid/NaN tracking samples, and no Windows crash event after deployment
- The release remains Win32 for compatibility with the original engine dependencies and bundles app-local OpenVR, OpenAL Soft, and Visual C++ runtime DLLs. Windows 10/11 users do not need a separate VC++ redistributable or system OpenAL installation

### Known limitations

- The collision proxy represents the palm/hand, not individual fingers. Animated fingers and very thin geometry can still clip visually
- Complex concave contacts can temporarily constrain a hand; pulling the controller back toward the body triggers bounded recovery. Continuing to push into the obstacle intentionally does not snap or bypass it
- PS VR2 Sense is still the only controller family validated on hardware. Index, Touch, Pico, WMR, and Vive profiles are included through the shared SteamVR action layer but need broader device testing during beta
- The original global player state still supports one active grab at a time; simultaneous two-object holding and two-hand constraints remain future work

---

## v0.1.0-alpha.6.3 — 28 August 2026

**More reliable visible-item acquisition, middle-finger magnetic guidance, low-ceiling recovery, and a cheaper VR interaction hot path.**

### Added

- Assisted hand-aim pickup for visible inventory items: normal palm contact remains authoritative, while a distance/angle/type score and strict controller-plus-head line of sight select small items that are visible but difficult to reach between locker shelves; consumables receive the strongest range (up to 2.35 metres), important equipment requires closer intent, entering the acquisition cone gives a subtle one-shot haptic cue and a thin blue object-to-middle-finger line, and Interact uses the normal pickup sound, callback, and inventory path. Closed doors and other solid bodies block acquisition even if the physical controller has crossed them. The cue has its own world-render path and is suppressed during inventory/menu UI without changing their pointer laser

### Fixed

- Low ceilings now constrain the effective VR stance without changing the player's physical/button/hybrid crouch request; crouch input has one owner instead of passing through both the VR latch and legacy toggle, standing ignores harmless floor-only contact while testing the full-height character collider every frame, the virtual crouch offset remains active after a failed stand, and normal standing returns automatically as soon as there is headroom and no crouch input remains
- Small drawers are easier to acquire: the interaction volume follows bounded raw-controller intent beyond a collision-stopped visible palm while retaining strict solid-body line of sight, and an acquired mechanism keeps its real surface contact point for slider/hinge projection instead of using an imaginary point in the gap at the palm. The near-contact shortcut is disabled whenever that extension is meaningful, preventing items behind thin closed doors from being treated as palm contact
- Inventory pickup haptics now follow the hand that actually selected the item, including off-hand and assisted pickups

### Optimized

- Per-frame hand interaction no longer allocates a temporary vector merely to retain the nearest item and nearest mechanism. Magnetic acquisition queries a tight aim-segment volume and ray-tests only its five highest-ranked candidates instead of every eligible body. The posed-hand update also stops writing a recurring diagnostic log while an item is held

---

## v0.1.0-alpha.6.2 — 27 August 2026

**Physical hands that respect the world, measured equipment grips, and much more forgiving doors, drawers, hatches, pickups, and opening-area mechanisms.**

### Fixed

- Visible hands now sweep an approximate physical volume through static scenery, resolve overlaps, and slide along surfaces instead of freely crossing walls and furniture; correction lag is bounded so a deeply blocked pose cannot strand or permanently hide a hand
- Tracking loss is isolated per hand and has a short grace period, preventing one transient invalid pose from removing both rigs
- Finger fold axes turn into the palm consistently on both rigs; the little finger no longer rotates backwards (the short thumb chain remains static by design)
- Flashlight, glowstick, hammer, pickaxe, broom, flare, and dynamite placement is calibrated against reconstructed object geometry and the hand's closing-finger volume instead of a fixed visual palm offset
- VR interaction selects the joint whose child body was actually grabbed, so the opening snow hatch drives its lid joint rather than the wheel joint and is substantially easier to lift
- Inserted handles forward interaction to the mechanism they drive: the steel bar near the opening hatch can now turn its support after placement instead of requiring the player to grab the support itself
- Hinges use circular pick-point motion and sliders project onto their permitted axis, making lockers, drawers, doors, wheels, and levers less resistant and less prone to drifting off-axis
- Locked swing doors retain their hinge axis and stop around their locked angle, including the tutorial door blocked by its padlock
- Small and light pickups, including inventory items inside cramped lockers, receive stronger hand-nudge assistance; heavy, large, or breakable props receive a restrained response so bare hands do not casually explode barrels or punch loaded crates across the room
- Held objects follow the collision-resolved physical palm pose while targeting and direct nudge preserve the raw controller intent
- Locomotion no longer computes the head-rotation inverse twice per frame
- The sound source filter is only pushed to the driver when it actually changes, instead of re-issuing identical state for every audible sound each update
- VR eye framebuffer descriptors are zero-initialized, so a failed allocation attempt cannot destroy unrelated GL object names

### Added

- Per-item `VrGripPoint`, `VrGripRadius`, and `VrGripTwist` metadata: the attachment aligns a measured grip point with the closing fingers, derives curl from handle radius, and twists around the grip pivot
- `scripts/analyze-vr-grips.py`, which reconstructs HUD-object geometry and hand skeleton poses, reports grip slices and bounds, and validates anchors without requiring repeated in-headset guesswork
- Scale-free physical palm transforms and contact-distance prioritization for world selection, independent of the tiny visual mesh scale
- Joint-selection and nudge diagnostics for map-specific interaction tuning
- Release identifier (`Penumbra VR vX.Y.Z`) logged at the very start of every boot, independent of the SteamVR input path
- Environment fingerprint in `hpl.log`: Windows build, RAM and free 32-bit address space, real GPU vendor/renderer/driver — plus remaining address space recorded at crash time
- Controller bindings are now generated from a single declarative spec (`scripts/generate-bindings.ps1`); the build fails if the shipped JSONs drift from it
- Unit tests for the pure VR math (tracking-space transform, recenter invariant, stick dead zone) run on every build and in CI

### Known limitations

- Hand collision uses one approximate palm/hand volume, not individual physical fingers; some clipping around thin geometry, handles, and animated fingers remains possible
- The current hand asset has a short static thumb chain and a fused ring/middle mesh, so it cannot produce a complete anatomical wrap around every handle
- The original global player state still permits only one active grab; simultaneous two-object holding and two-hand constraints are not implemented
- Bundled equipment is calibrated individually, but unusual or map-specific held meshes may still need their own grip metadata

---

## v0.1.0-alpha.6.1 — 26 August 2026

**Hotfix release: startup crash resilience on upgraded installs, occlusion filtering that actually reaches OpenAL, self-service crash reporting, and a packaged-in Visual C++ runtime.**

### Fixed

- Upgraded installations no longer re-enable the legacy sound updater thread: builds up to alpha.5 wrote `[Sound] UseThreading=true` into `settings.cfg`, silently racing OpenAL Soft's internal mixer once EFX was active (startup crash around the first menu sounds). The stored value is now ignored with a logged warning; the thread stays off for everyone
- Occlusion muffling and distance air absorption reach OpenAL for real: source filters created on demand had no type, so every `SetGainHF` call was rejected and alpha.6's advertised low-pass behaviour never applied
- VR eye framebuffer allocation failures (driver limits or the 32-bit address space at high render scales) now verify framebuffer completeness and retry at half size instead of leaving rendering broken; eye descriptors are zero-initialized before any failed attempt destroys partial GL objects

### Added

- Automatic crash reporting: unhandled exceptions write `penumbravr_crash.dmp` next to the executable and a flushed `CRASH:` line into `hpl.log` — support requests only ever need files the game produced by itself
- Stale-binding diagnostics: when move/turn actions stay inactive while other controls work (a SteamVR binding stored by an older alpha; reported on Valve Index), the log explains the one-time fix — SteamVR ▸ Settings ▸ Controllers ▸ Reset to default — and the troubleshooting guide documents it
- App-local Visual C++ 2022 runtime (`msvcp140.dll`, `vcruntime140.dll`) ships inside the package; players no longer need the redistributable installed

### Known limitations

- SteamVR still prefers bindings it saved on first launch over newer bundled defaults; the mod detects and reports the situation but cannot override the user's stored profile programmatically
- Environmental reverb remains a single global preset; per-zone environments remain future work

---

## v0.1.0-alpha.6 — 25 August 2026

**A complete spatial-audio chain: binaural HRTF, occlusion muffling, distance absorption, ambient reverb, and a low-latency head-tracked listener — on a bundled OpenAL Soft runtime.**

### Added

- Bundled OpenAL Soft 1.25.2 (Win32) as an app-local library: guaranteed EFX and HRTF support on every machine regardless of the installed OpenAL runtime; LGPL license shipped under `licenses/`
- Binaural audio (HRTF) row in Options → VR Settings → Display with **Auto** (the runtime enables it only when Windows reports headphones), **Headphones** (forced), and **Off**; persisted as `[VR] HRTF` in `settings.cfg` and applied through an app-local `alsoft.ini` written at startup — restart required, like render scale
- Global environmental reverb tuned for Penumbra's mine galleries: restrained wet mix (bus trim 0.32), 2.6 s decay whose treble falls faster than its body, high diffusion; routed through EFX for world sounds only, leaving UI and streams untouched
- Occlusion muffles instead of just attenuating: blocked sounds drive a per-source low-pass filter that follows the same fade curve as the volume cut (HF gain 0.2 when hidden → 1.0 in the open)
- Distance air absorption on the same filter: audible sources lose treble progressively down to ~55% HF gain at max range, so far things read as far before any wall is involved
- Spanish localization for the new setting

### Fixed

- The VR audio listener follows the tracked head pose of the current frame instead of the previous frame's camera matrix, removing a one-frame pan lag on positional sounds during fast head turns and after recentering
- Device opening no longer forces legacy Creative router names ("Generic Software") that modern runtimes reject; unknown or unset names open the runtime's default output
- OALWrapper hardened for first-time EFX activation: per-source filters are created on demand instead of dereferencing a permanent NULL (startup access violation), the EFX manager initializes its thread state before any failure path can tear it down, and failed device opens no longer route into the silent NULL driver
- Legacy wrapper sound thread disabled by default — it races against OpenAL Soft's internal mixer once EFX is active; OpenAL Soft already mixes on its own thread

### Known limitations

- Environmental reverb is a single global preset; per-zone or map-driven environments remain future work
- HRTF has been validated informally on PS VR2 Sense headphone output only; other headsets follow the same OpenAL path but lack a dedicated hardware pass
- Occlusion raycasts still sample every 30 updates (~0.5 s); very fast doors can pop between muffled and clear states

---

## v0.1.0-alpha.5 — 25 August 2026

**Natural hand interaction, firm tracked grabs, and off-hand grabbing.**

### Added

- Bare off-hand interaction on PS VR2 Sense: L2 in the default right-handed profile and R2 in the mirrored left-handed profile can target and grab nearby objects
- Off-hand trigger bindings in every bundled controller profile (HTC Vive, Valve Index/Knuckles, Oculus Touch, Pico 4, Pico Neo 3, Windows Mixed Reality): each hand's index trigger maps to `/actions/offhand/in/interact`, matching the PS VR2 scheme; controller profiles ship in parity from now on
- Dedicated `/actions/offhand` SteamVR action set, restricted at runtime to the physical off-hand controller without activating the complete mirrored gameplay layout
- Direct hand nudge for dynamic world objects and hinged doors, with contact-point impulses and subtle per-hand haptics
- Per-hand interaction-source and held-body ownership so tracking, visibility, throwing, and haptics follow the controller that initiated the grab
- Throttled input and nudge diagnostics in `hpl.log`

### Fixed

- Held physics objects now remain fixed to the rendered palm instead of trailing, floating, or orbiting around the controller
- Map-defined Newton velocity caps are raised only while grabbing or dragging and restored on release; jointed bodies use a bounded velocity servo
- Throw velocity is transformed into world space and clamped to reject controller spikes
- Create/recenter remains available in both dominant-hand modes through the global action set
- Pickup items touched by the palm take priority over overlapping furniture, while normal line-of-sight checks remain in place away from direct contact
- Item-use rays now follow the aim pose rotation; the item indicator is smaller, view-facing, and anchored to the ray origin
- Hand nudge excludes inventory items and held bodies, and applies gentler limits to grabbable/movable props so pickups are not launched accidentally

### Known limitations

- Interaction still uses one global player state: both hands can initiate a grab, but simultaneous two-object holding and two-hand constraints on one object are not implemented
- Hand nudge is an impulse-based proxy for dynamic bodies, not a rigid hand collider against static level geometry; rendered hands can still pass visually through walls
- Off-hand grabbing has been hardware-tested on PS VR2 Sense only; other bundled profiles use their standard SteamVR trigger inputs for off-hand interact but remain unvalidated per device

---

## v0.1.0-alpha.4 — 24 August 2026

**Graphics presets and first pass of equipped objects in hands** (experimental, very green state).

### Added

- `[VR] RenderScale` setting (0.5–2.0, default 1.0) — multiplies SteamVR recommended eye buffer size at startup; main performance lever on modern headsets (restart required)
- One-click graphics presets in Options → VR Settings: Performance (0.75×, shadows off, medium shaders, 4× anisotropy), Balanced (1.0×, static shadows, 8× anisotropy), Quality (1.25×, full shadows, 16× anisotropy)
- Texture anisotropic filtering now defaults to 16× instead of Off
- Equipped objects render as attachments on visible hand rigs: flashlight, glowstick, flare, melee weapons (hammer, pickaxe, broom), thrown items
- Items anchor to palm socket in controller frame with per-item rotation/scale
- Melee weapons and thrown items keep full gameplay logic (damage sweeps, charging, throw release)
- Notebook draws without depth test — world geometry no longer clips it

### Fixed

- `[Screen] Vsync` never being loaded and `[Graphics] Refractions` never being saved
- Crash when opening notebook while flare was equipped (use-after-free on flare helper pointer)
- Thrown item honours `VrScale` of its hud file again (held dynamite override at 6.0)

### Known limitations (held objects)

- Forced grip does not close fist completely; finger wrap is shallow, thumb stays static
- Per-item placement is a first pass — some items intersect hand or sit off palm line
- System may change without notice between builds

---

## v0.1.0-alpha.3 — 23 August 2026

**Controller coverage and input resilience fixes** (driven by first external Valve Index tester report).

### Added

- Controller diagnostics in `hpl.log`: detected headset driver and controller types recorded at startup with bundled profile match status; unmatched types log explicit warning
- Legacy polling fallback drives movement from strongest qualifying analog axis (joystick-only devices like Index, WMR keep usable movement without action binding)
- New default binding profiles: `pico4_controller`, `pico_neo3_controller` (Pico 4/Ultra, Neo 3 via SteamVR bridges, mirroring Touch layout), `holographic_controller` (WMR identifier, shares existing WMR scheme)
- SteamVR Input dropouts bridged for 500 ms with last known input state before falling back to legacy polling
- Game messages re-anchor in room space when another UI surface closes

### Fixed

- SteamVR flagging every action origin inactive in short bursts no longer swallows held inputs on controllers whose sticks don't assert legacy touch bit

---

## v0.1.0-alpha.2 — 22 August 2026

**Natural hand animation and aiming polish.**

### Added

- Per-finger hand animation driven by SteamVR skeletal summary: on PS VR2 Sense, Index, Touch, every finger follows its own measured curl
- Devices without skeletal input synthesize natural closing sequence from grip/trigger: pinky/ring start first, middle follows, index tied to trigger
- All finger weights pass through exponential smoother (~70 ms) with soft deadzone: hands rest fully open, joints move with slight inertia
- Fold axes corrected per hand (left rig is Y-mirror of right)
- One plain ±Z fold axis shared by four fingers — removes widened double middle finger from diverging anatomical axes with fused Ring/Middle mesh
- Pinky folds at exact Middle/Ring parity with axis tilted 15° away from ring lane
- Thumb frozen at bind pose by decision (rig chain too short, no sweep direction read as visible bending on hardware)
- Interaction laser uses SteamVR aim pose with automatic grip-pose fallback

### Known limitations (hands)

- Ring/Middle share fused mesh tube — tips cannot spread independently
- Thumb chain too short to reach palm flesh — stays static during grips
- Legacy devices (Vive wands, WMR polling fallback) follow synthesized grip sequence only

---

## v0.1.0-alpha.1 — 20 August 2026

**Experimental public alpha. Full game playable in VR.**

### Added

- Device-independent SteamVR Input actions (gameplay, UI, global sets) with automatic legacy OpenVR polling fallback
- Single rigid TrackingToWorld transform for HMD, both hands, held objects, world-space UI; recenter; snap/smooth/disabled turn through world yaw
- Persistent VR comfort settings in `[VR]` section of `settings.cfg`, editable in-game under Options → VR Settings (CONTROLS, MOVEMENT, CALIBRATION, DISPLAY sections with contextual disabling, one-press Recenter row)
- Dominant-hand selection (Left/Right) — reselects mirrored action set, drives interaction, pointer, throw, haptics
- Seated and standing play modes; player height with Calibrate button (measures HMD height, rejects implausible samples)
- Physical, button, and hybrid crouch driving real gameplay collider; physical crouch calibrates standing HMD height with plausibility band (0.90–2.20 m)
- Room-anchored fullscreen menus and cinematics with scalable text; compositor-safe black loading frames for map changes, new game, tutorial, saved-game routes
- Controller-ray menu and inventory pointing, restored inventory action popups, localized inventory actions
- Semantic haptics routed through intent-named profiles with per-event repeat limits
- Room-scale collision reconciliation for swing doors (no walking through while leaning) and held-door collision bypass fix
- Rigged HUD hand models animated from SteamVR skeletal summary (grip/trigger finger curls)
- Reversible installer — merges mod into Steam installation, backs up replaced files, restores on request (`Install-PenumbraVR.bat` installs, `-Restore` uninstalls)

### Fixed (post-initial-download)

- Recenter with Create button now works in right-handed mode (movement input was overriding recenter flag, dropping idle Create press into legacy fallback which has no Create on PS VR2)
- Interaction ray uses SteamVR aim pose on every profile with automatic grip-pose fallback
- Thrown/released objects inherit real hand velocity rotated into world space
- Death screen, examine/gameplay messages, radio subtitles anchored at UI Distance setting
- Equipped HUD items no longer render collision-only submeshes; flashlight beam origin at lens
- Easier grabbing: larger hand interaction volume, firmer held-object feel

### Known limitations

- Hand fold state is experimental and shows incorrect visual behaviours (ring/middle fused tube, limited index range, basic grip/trigger blend poses)
- PS VR2 Sense is the only controller family validated on hardware
- Dominant-hand, play-mode, and player-height settings implemented but not fully hardware-validated
- Build is Win32 (legacy third-party binaries); engine rendering keeps OpenVR compositor path
- No guarantee of behaviour across graphics drivers or unusual Windows setups
