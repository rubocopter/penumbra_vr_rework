# Release history

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
