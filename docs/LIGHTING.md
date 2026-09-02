# Rendering and lighting research

This document records the lighting investigation performed against the current
HPL1/OpenVR renderer, including the discarded SSAO prototype, the two
ambient-only headset tests and the broader Enhanced visuals A/B that followed.
It separates verified current behaviour from experiments so a prototype cannot
silently become a shipping renderer dependency.

## Current status

- There is no SSAO pass, depth sampling, contact-shadow pass or ray tracing.
- Each VR eye still exposes an `RGBA8` compositor texture and packed
  `GL_DEPTH24_STENCIL8` renderbuffer. The original stencil shadow-volume path is
  intact.
- With Enhanced visuals Off, the eye world renders directly into that original
  target with the original ambient and direct-light programs.
- With Enhanced visuals On, the eye world uses an auxiliary `RGBA16F` target
  with two-sample colour/depth/stencil, then resolves through a single final
  image pass. The compositor still receives SDR `RGBA8`; this does not claim
  HDR headset output.
- The monitor renderer is unchanged.
- The SSAO feasibility prototype was removed after hardware evaluation showed
  too little visible benefit.
- The ambient-only prototype also remained too subtle after its strong second
  test. Its shader is retained inside an Off-by-default VR-only bundle with
  point/spot response for baked dark texels, MSAA 2× and final image treatment.
- The first complete bundle was positively evaluated on PSVR2. A longer
  surface-refinement test found it too dark; the subsequent **soft-toe v2**
  went too far in the other direction, especially with held lights. The prior
  dark-curve/held-light revision was judged better, but the office still showed
  washed-out flashlight surfaces and slightly insufficient ambient. Current v4
  keeps that final curve, reduces diffuse recovery/specular boost and lifts only
  the ambient cap slightly. The latest v4 headset feedback was positive
  ("está mejor"); the subsequent multi-map run supports the stable baseline.
  Matched timing and exhaustive regression checks remain open.
- A 231-image bounded-resolution diffuse selection now accompanies `v0.1.0`
  packages, preserving the original nine-image trial and expanding it in one
  grouped test, reported successful on PS VR2. This asset-only follow-up does
  not change the v4 shaders. See the [session record](VALIDATION-v0.1.0.md).
  They change assets in both On and Off, not the monitor/Off renderer. See
  [Texture selection](TEXTURES.md) for exclusions, memory estimates and rollback.

### Current calibration: dark curve, bounded ambient and held lights

The restored dark curve improved the next headset test, but objects under the
flashlight in the office still looked washed out while the room itself remained
a little too dark. V4 targets these separately, without another global exposure
or gamma change. The final VP/FP and both glowstick halo FPs are byte-identical
to those in the user's tested AD0ED41B executable package.

- The final pass restores the earlier exact RGB transform: exposure `1.25`,
  component-wise rational curve with toe `0.03`, saturation `1.12`, contrast
  `1.08` about `0.5`, gamma exponent `0.94`. Five-tap sharpening remains bounded;
  final alpha remains the centre sample. This deliberately restores the former
  dark toe, rather than claiming to eliminate black crushing globally.
- Only the VR `RenderZ` shader preconditions ambient: `a * 0.38 + min(a * 3,
  0.031)`, where `a = diffuse.rgb * ambientColor * response`. The cap was `0.028`
  in the tested build: this is at most `+0.003` pre-tone RGB, not a global boost.
  Zero stays zero and no direct light or emissive uses it.
- Vertex hemisphere response is `0.65–0.90`, direction `0.90–1.00`, with ground
  `(1, .99, .98)` / sky `(.98, .99, 1)` tints. Every response channel is at most
  `0.90`. The preconditioned *pre-tone* ambient can exceed its old numeric value;
  the intended constraint is its darker **displayed** result after the restored
  curve. CPU reference cases verify this against original Off RGB for uniform,
  ambient-only surfaces. This is not a pixelwise guarantee for mixed lighting,
  sharpening edges, fog, or arbitrary HDR map-script ambient values.
- Held Glowstick light RGB is multiplied by `0.70`; Flashlight by `0.90` at
  uniform upload, only during Enhanced VR draws. Other lights default to `1`.
  Alpha/specular mask, equip fades, simulation colour, radius, culling,
  projection, alignment and stencil-volume inputs are unchanged. These are
  source-light multipliers, not promised percentages of perceived brightness.
- The held glowstick halo is tagged at creation. Only its Enhanced VR draw
  substitutes an analytic RGB envelope: `max(1-dot(2*uv-1,2*uv-1),0)^3` with
  peak RGB `(.030,.075,.055)`, multiplied by existing billboard colour/visibility.
  It removes the JPEG's RGB rings, retains sampled alpha, and uses the original
  fog texture/setup when fog is active. Size, depth test, blend and occlusion
  queries remain unchanged. Both halo programs must load, otherwise the stock
  material is used. Finite SDR output may still show quantization; headset
  confirmation of smoothness and strength is pending.
- Per-vertex normal response and the four analytic light-direction replacements
  survive v2. No new fullscreen pass, framebuffer, blur, bloom, SSAO or texture
  sample is added to the existing light/final paths. Restoring RGB gamma does
  restore three scalar powers instead of v2's one; cost must be measured again.
- V4 reduces the gray diffuse-recovery coefficient from `0.18` to `0.06` in
  all ten enhanced point/spot programs. The former mapping retained only about
  36% of neutral texture contrast below luma 0.28; the new one retains about
  79% before lighting/tone. Non-dark diffuse is untouched. This is a plausible
  source of the reported washout, not proof from a captured GPU image.
- The six specular programs retain exponent 24, masks and NdotL gating but
  drop their `1.35` peak boost to `1.00`. Plain diffuse materials never use that
  specular term, so specular alone cannot explain every washed-out shelf.
  These material-response adjustments affect real point/spot lights generally,
  not just the flashlight. No texture, normal, shadow-volume input, FBO, sample
  count, halo, source-light gain, final image curve or user setting changes.

The hot toggle, Off default, two-column settings layout, exact renderer fallback
and per-eye stage timers remain intact. Log identification is now
`VR visual calibration: dark SDR curve v4, ambient cap 0.031, diffuse recovery 0.06, specular gain 1.00`.
V4 is now the current visual comparison baseline following the positive user
report, not a claim that every individual condition below has passed.
Remaining release checks: same lit room with neither/each held light; dark cave;
halo against dark and bright backgrounds; fog, equip fade and save/reload;
flashlight shadow silhouette; On/Off timings at unchanged scale 1.00.

## How HPL1 currently produces ambient light

`cRenderer3D::RenderWorld()` builds the opaque scene in this order:

1. `RenderZ()` writes scene depth and the base ambient colour into RGBA; the
   same packed depth/stencil attachment remains bound for the later passes.
2. Occlusion queries run with colour writes disabled.
3. `RenderLight()` adds point and spot lighting, including the flashlight and
   its stencil shadow volumes.
4. Illumination textures, fog, sky and the remaining passes are rendered.

Despite its name, `RenderZ()` is not depth-only. For the normal BaseLight
material family it selects `Diffuse_Color_vp.cg` and `Ambient_Color_fp.cg`,
binds the diffuse texture, uses replace blending and writes RGBA. The fragment
shader is effectively:

```text
output = diffuseTexture
output.rgb *= globalAmbient * sectorAmbient
```

The alpha sampled from the diffuse texture is not modified. No normal is read
by either ambient shader, so every pixel of one textured surface receives the
same ambient multiplier regardless of whether it faces up, sideways or down.
Normal maps remain exclusive to the later direct-light pass.

The apparent ambient values are also reduced before reaching the shader:

- `SetAmbientColor(r, g, b)` subtracts `0.0625` from every channel and clamps at
  zero.
- `SetSectorProperties(...)` stores each sector ambient channel multiplied by
  `0.85`.
- A sector without an override uses white, leaving only the global multiplier.

This explains why a map value near ten per cent leaves little energy for an
ambient-only occlusion effect to remove, especially once the flashlight or
another direct light is added afterward.

## VR framebuffer and stencil boundary

`CreateFrameBuffer()` creates the two independent compositor eye targets. Each
contains:

- one sampleable `GL_RGBA8` colour texture attached to
  `GL_COLOR_ATTACHMENT0`;
- one `GL_DEPTH24_STENCIL8` renderbuffer attached to the combined
  `GL_DEPTH_STENCIL_ATTACHMENT`.

The framebuffer is accepted only when `glCheckFramebufferStatus()` returns
`GL_FRAMEBUFFER_COMPLETE`; oversized eye buffers are retried at a smaller size.
HPL1 accesses the OpenGL framebuffer entry points through GLee. Nothing in the
shadow-volume algorithm requires the attachment to be a renderbuffer: it
requires a working stencil attachment. Nevertheless, the current renderer
deliberately keeps the original renderbuffer because no active effect samples
depth.

The Enhanced visuals path allocates a separate two-sample `RGBA16F` colour
renderbuffer and a two-sample `GL_DEPTH24_STENCIL8` renderbuffer for each eye,
plus a single-sample `RGBA16F` resolve texture. Shadow volumes operate on the
multisampled packed stencil attachment exactly where the world is rendered.
The world, hands, pointer and VR UI remain in that same multisampled target so
depth-tested overlays retain the world depth. Once all eye content is complete,
colour resolves to the float texture and receives the final RGB transform. This
avoids a non-portable multisample-to-single-sample depth/stencil blit; alpha is
sampled from the resolved eye and copied unchanged by the final shader.

The auxiliary path is enabled only when the final shaders load, float textures
and ARB/core framebuffer objects are present, the driver reports at least two
samples and both eyes produce complete framebuffers. Any failure destroys the
partial auxiliary objects and keeps the exact direct-to-`RGBA8` route. The
output texture identity and OpenVR `ColorSpace_Gamma` submission are unchanged.

During the SSAO experiment the same packed format was allocated as a depth/
stencil texture and attached to the same combined attachment. The original
renderbuffer remained the fallback whenever texture allocation, attachment or
FBO completeness failed. This proved the path feasible on the test system
without making a sampleable depth texture a startup requirement. That code is
no longer present.

## Discarded SSAO feasibility prototype

### Question being tested

Could a very small screen-space effect add useful contact definition while
touching only the ambient result between `RenderZ()` and `RenderLight()`?

The prototype was intentionally constrained to:

- VR eyes only; no monitor-renderer changes;
- packed `GL_DEPTH24_STENCIL8` texture with automatic fallback to the original
  renderbuffer;
- half-resolution SSAO;
- eight samples, one radius and no temporal noise, blur, bilateral upsampling,
  second radius or quality controls;
- the projection belonging to the eye currently being rendered;
- multiplication of RGB ambient only, with alpha unchanged;
- a hot runtime toggle, default Off;
- separate left/right GPU timer queries.

The half-resolution AO target was reported as `1598 × 1629` on the test setup.
Measured pass cost was approximately:

| Eye | GPU time |
| --- | ---: |
| Left | 0.24 ms |
| Right | 0.22 ms |
| Combined | 0.46 ms per stereo frame |

These figures describe the discarded prototype on one test setup, not a general
performance guarantee.

### Diagnostic observations

The raw AO mask was temporarily displayed in greyscale. It correctly revealed
corners, contacts and some vertices, which established that depth access,
per-eye reconstruction and sampling were functioning. The mask was nevertheless
almost uniformly white and its useful contrast was narrow and subtle.

Occlusion-only contrast was then amplified while preserving `1.0` for
unoccluded pixels, avoiding a simple global brightness reduction. Stronger
fixed multipliers made the existing contacts a little darker but did not create
a meaningful increase in perceived surface volume. In normal lighting the main
visible change was a slight darkening of areas that already read as shadowed.

The A/B comparison was performed in the headset with the normal runtime toggle
and diagnostic mask/strength modes. No canonical comparison captures were kept
in the repository, so the observations above are the retained result rather
than a claim of image-based regression coverage.

### Decision and cleanup

The implementation was technically valid and inexpensive in isolation, but
its visual value did not justify approximately `0.46 ms` of stereo GPU work or
the extra framebuffer and shader complexity. It was therefore rejected for the
current renderer.

The depth texture, AO targets, shaders, composite pass, timer queries, F8/F9
diagnostics, configuration key, translations and VR-menu setting were all
removed. Runtime-code, shader and packaged-resource searches contain no SSAO
remnants. The eye FBO is back to the packed depth/stencil renderbuffer and the
flashlight's existing stencil path was not modified.

## Alternatives considered

| Candidate | Status | Reasoning |
| --- | --- | --- |
| Half-resolution SSAO | Rejected for now | Valid mask and low absolute cost, but negligible perceived improvement in the tested scenes |
| Hemispherical ambient | Retained inside broader A/B | Both standalone headset tests were too subtle; still nearly free and useful as one layer rather than the headline effect |
| Soft ambient direction | Retained inside broader A/B | Adds separation between opposing vertical walls with one vertex dot product and one interpolator |
| Separate sky/ground colours | Retained inside broader A/B | Uses fixed warm ground and cool sky multipliers; no textures or additional controls |
| RGBA16F + MSAA 2× | Implemented for A/B | Preserves additive-light headroom and antialiases geometry/stencil edges before the final SDR resolve; falls back as one unit |
| Final image treatment | Recalibrated after both extremes | Restored dark five-tap treatment; readability is adjusted through bounded ambient rather than a global soft-toe lift |
| Baked-shadow light response | Implemented for A/B | Point and spot lights can lift nearly black diffuse texels while their real attenuation, projection and stencil visibility remain authoritative |
| Fake contact shading | Low priority | The SSAO test indicates that more contact darkening is not the main visual deficit |
| Distance attenuation/fog | Deferred | Potentially useful for depth separation but much more likely to change Penumbra's original atmosphere |
| Native HDR headset output | Rejected | OpenVR submission remains gamma-space `RGBA8`; internal `RGBA16F` exists only to retain headroom for an SDR final transform |

## Earlier enhanced-ambient A/B

The implemented test alters only the existing ambient term:

```text
hemi = saturate(normalWorld.y * 0.5 + 0.5)
direction = saturate(dot(normalWorld, normalize(0.35, 0.85, 0.40)) * 0.5 + 0.5)
orientationFactor = lerp(0.35, 1.15, hemi)
directionFactor = lerp(0.65, 1.35, direction)
ambientTint = lerp((1.08, 0.90, 0.76), (0.88, 1.02, 1.18), hemi)
output.rgb = diffuse.rgb * globalAmbient * sectorAmbient
             * orientationFactor * directionFactor * ambientTint
output.a = diffuse.a
```

The first headset build used deliberately conservative `Up = 1.0` and
`Down = 0.75` values. It produced the expected orientation response, but the
result was barely perceptible: vertical surfaces retained 87.5 per cent of an
ambient component that is already only about one tenth of the final colour,
and direct flashlight lighting masked most of the remaining difference.

The second A/B kept exactly the same render path but deliberately grouped the
three nearly free candidates so they could be judged in one headset run. Its
vertical endpoints are `Up = 1.15` and `Down = 0.35`, with `0.75` on vertical
surfaces. A normalized world direction `(0.35, 0.85, 0.40)` then supplies a
`0.65`–`1.35` multiplier so opposing walls no longer receive identical
ambient. Finally, fixed warm-ground `(1.08, 0.90, 0.76)` and cool-sky
`(0.88, 1.02, 1.18)` multipliers give the orientation difference a visible
colour cue. Even at these intentionally broad values, the headset result was
still reported as difficult to distinguish. That closed ambient-only shading
as the primary visual upgrade; the shader remains useful only as one cheap
component of the broader comparison below.

### Implementation boundary

1. The shared `Diffuse_Color_vp.cg` and `Ambient_Color_fp.cg` remain untouched.
   Off and monitor rendering select those exact original program objects.
2. `Ambient_Hemisphere_vp.cg` and `Ambient_Hemisphere_fp.cg` form a dedicated
   alternative pair for the BaseLight Z pass only.
3. The vertex shader receives the geometry normal through an inverse-transpose
   object-to-world normal matrix. It derives both the world-Y hemisphere and
   the soft directional response per vertex; a singular object matrix falls
   back to identity instead of producing invalid normals.
4. The compiled render state retains both original and VR-alternative program
   pointers. Selection occurs at draw-state application time because one render
   list is shared by the optional monitor view and both eyes. The alternative
   is selected only while the low-level renderer marks an eye as VR and the
   runtime toggle is On.
5. Both alternative shaders must load. If either fails, both alternative
   pointers are discarded and state selection automatically uses the original
   pair.
6. At this stage, `RenderLight()`, direct-light shaders, stencil operations, eye
   FBOs and the monitor path were untouched.

HPL1 already exposes vertex normals and per-object program setup hooks;
`Material_EnvMap_Reflect.cpp` demonstrates sending `objectWorldMatrix` during a
draw. Computing and interpolating both orientation factors in the vertex shader
keeps the fragment work to fixed lerps and multiplies. Passing an
inverse-transpose normal matrix is safer than taking the object's upper-left
3×3 directly because non-uniform scale can otherwise skew the result.

The prototype covers the normal BaseLight material family. Legacy fallback or
unusual material paths retain the original ambient shader rather than receiving
a broad modification to a shared program.

The old `[VR] HemisphericalAmbient` value is now read only as a migration
fallback. Saving writes `[VR] EnhancedVisuals`; the same menu row is exposed as
**Options → VR Settings → Display → Enhanced visuals** (`Mejoras visuales` in
Spanish).

## Enhanced visuals A/B bundle

The current experiment intentionally combines the remaining low-cost changes
in one visibly stronger comparison. It is not presented as final calibration.
The goal is to decide whether modernizing HPL1's whole VR image is worthwhile
before spending more headset runs on small individual coefficients.

### World target and MSAA

- The original `RGBA8` compositor targets are always created first.
- When supported, each eye also receives an `RGBA16F` two-sample colour
  renderbuffer, a two-sample packed depth/stencil renderbuffer and an
  `RGBA16F` resolve texture.
- The entire opaque/transparent world and subsequent per-eye VR drawing use
  that multisampled target. Stencil shadow volumes and depth-tested UI therefore
  share the same packed attachment and are not replaced or approximated.
- Completed eye colour resolves to the float texture; depth and stencil need no
  cross-sample-count copy and remain private to the multisampled render.
- If extensions, sample count, shaders or either eye framebuffer fail, the
  auxiliary objects are discarded and both eyes use the original target.

This is internal HDR headroom, not HDR display output. OpenVR still receives
the original gamma-space `RGBA8` texture and no headset HDR capability is
assumed.

### Direct-light response and baked shadows

The black regions reported under lamps are consistent with lighting baked into
legacy diffuse textures. Every original point/spot shader computes direct
light by multiplying the diffuse sample; a texel at or near black cannot become
bright regardless of how much valid flashlight light reaches it.

The VR alternatives keep the original light colour (apart from the explicit
held-light RGB gains described above), projection texture,
distance attenuation, normal/specular maps and stencil visibility. Before that
lighting multiplication they lift only dark diffuse samples:

```text
luma = dot(diffuse, (0.299, 0.587, 0.114))
diffuse += (1 - saturate(luma / 0.28)) * 0.06
NdotL = saturate((NdotL + 0.08) / 1.08)
```

The initial `0.18` floor was deliberately strong for the A/B; it is now `0.06`
after the washed-out-office report. The small wrapped
Lambert term makes surface response easier to read near grazing angles. It is
applied only where a real point or spot light contributes; stencil-shadowed
pixels still receive no contribution. Five high-quality BaseLight families
(Diffuse, Bump, DiffuseSpec, BumpSpec and BumpColorSpec) have matching point
and one-pass spot variants. A missing alternative program falls back to that
material's original light shader.

### Follow-up: surface relief and masked highlights

The surface-refinement test initially kept the first bundle's ambient and tone
constants. Its following direct-light refinements remain in soft-toe v2:

- Bump, BumpSpec and BumpColorSpec (point and spot) scale decoded tangent-space
  normal XY by `1.65`, floor Z at `0.05` and normalize. A flat normal remains
  flat; a zero/degenerate sampled vector remains finite. No height map,
  parallax, new texture, geometry change or extra pass is introduced.
- Already-specular families use a `24` exponent and `1.00` peak multiplier
  (initially `1.35`, reduced in v4) in
  place of the original `16` exponent. Existing normal-alpha or coloured
  specular masks still control which texels receive highlights; non-specular
  families do not acquire reflections.
- Highlights are multiplied by `saturate(NdotL * 4)` using the **unwrapped**
  normal/light dot product, so they fade at grazing incidence and vanish on
  back-facing normal-map facets. They now follow the actual light RGB and,
  for spots, projected RGB in addition to the original attenuation, alpha and
  negative-projection rejection. This is a controlled artistic refinement,
  not a PBR/roughness conversion.

The normal multiplier is deliberately noticeable for a grouped headset test.
Stronger relief and a narrower highlight can still shimmer on distant textures;
offline compilation cannot establish VR stability. Inspect a rough wall under a
moving flashlight and a specular prop from several viewing angles. No material
files, shared original shaders or stencil operations change. Soft-toe v2 only
adds analytic normalization to the non-specular light paths described below.

### Discarded calibration record — soft-toe v2

The following records the tested v2 implementation, **not the current curve**.
The user rejected its excessive brightness and preferred the earlier dark
treatment with a separate small ambient adjustment. Current values are above.

The resolved complete `RGBA16F` eye is sampled once with a five-tap cross:

1. unsharp mask strength `0.65`, bounded by the same five samples' per-channel
   minimum/maximum to suppress newly introduced bright/dark extrema;
2. take peak RGB and expose that scalar by `1.25`;
3. map it with `x * (2.51*x + 0.10) / (x*(2.43*x + 0.59) + 0.14)`;
4. clamp the mapped peak and apply one scalar gamma exponent `0.94`;
5. scale the original sharpened RGB by `displayPeak / max(peak, 0.000001)`.

The values are intentionally visible and fixed. There are no separate quality,
intensity, exposure or sharpening controls during this comparison. RGB is
clamped into the SDR output; the centre sample's alpha is copied exactly. The
transform covers the complete eye, including its VR UI, after all depth-aware
drawing has finished.

Mapping one peak and scaling all three channels by the same factor preserves
their ratios instead of independently whitening coloured lights. The previous
`1.12` saturation boost and `1.08` subtractive mid-grey contrast are removed.
The old curve used a `0.03` toe coefficient; together with that contrast it
discarded neutral RGB inputs up to approximately **0.03571** entirely. That was
black crushing, not an intrinsic benefit of the float target or OLED headset.

The new positive toe response retains dim information while mapping exactly
zero to zero. No additive grey/ambient floor or adaptive exposure is introduced.
Illustrative **neutral RGB** reference values, before 8-bit quantization:

| Input | Previous output | Soft-toe v2 output |
| ---: | ---: | ---: |
| 0.01 | 0.0000 | 0.0146 |
| 0.03 | 0.0000 | 0.0530 |
| 0.05 | 0.0343 | 0.0998 |
| 0.20 | 0.3868 | 0.4364 |
| 0.80 | 0.8375 | 0.8355 |

These are arithmetic checks, not measured headset luminance or image captures.
The transform remains artistic/display-referred rather than physically linear.

### V2 ambient and shader-work reductions (historical)

Soft-toe v2 groups the following changes into the existing hot toggle:

- Hemisphere endpoints become `0.70` down / `1.15` up (`0.925` on a vertical
  face), replacing `0.35` / `1.15`. Direction modulation narrows from
  `0.65–1.35` to `0.90–1.10`. Existing map ambient is multiplied, never replaced
  or supplemented by a constant light source: zero ambient remains zero.
- Ground/sky tints become `(1.04, 0.98, 0.94)` / `(0.96, 1.00, 1.06)`. The
  full RGB orientation/direction/tint response is evaluated in the vertex shader
  and interpolated. The ambient fragment shader now only samples diffuse and
  multiplies by that response and the existing ambient uniform. This deliberately
  changes interpolation on smooth curved meshes as well as the calibration;
  it is not claimed as a pixel-identical rearrangement of the old products.
- The four non-specular Diffuse/Bump point/spot shaders replace the legacy
  normalization-cube lookup with `lightDir * rsqrt(max(lengthSquared, 1e-6))`.
  This removes one texture instruction per light fragment in each affected
  program, avoids cube quantization and remains finite at a zero direction.
  Shared material bindings/cube resources remain because original/monitor
  programs still use them. Falloff, projection and stencil tests are unchanged.
- Offline `fp40` output confirms that the final pass uses **one** scalar power
  instead of three, while retaining five texture reads. Normal-mapped relief,
  masked specular response and sharpening bounds from the last test remain.

These are modest arithmetic/lookup savings, **not a measured frame-time saving**
and not a claim that the existing float/MSAA bundle is free. No additional pass,
framebuffer, texture, sample count, blur, bloom, fog or SSAO is introduced.
Anisotropy was already at the reported hardware maximum (16×) in the reviewed
configuration. Personal graphics settings and the extra monitor view are not
changed automatically.

### Runtime and measurement contract

- `Enhanced visuals Off` is the default and binds the original eye FBO and
  original ambient/direct-light programs. No neutral version of the new final
  shader is executed.
- `Enhanced visuals On` enables the complete available bundle on the next eye,
  without restarting or reloading the map.
- The monitor always uses its existing framebuffer and original material
  programs.
- SSAO, depth sampling, temporal history, blur, bloom and extra quality options
  remain absent.
- When `GL_EXT_timer_query` and a nonzero elapsed-time counter are available,
  asynchronous double-buffered queries collect three **consecutive, not
  nested** intervals: scene/UI, MSAA colour resolve, and final image treatment.
  Off records only scene/UI; its resolve/final values are zero because those
  passes do not run. Measurement support never gates visual support.
- `hpl.log` reports independent 120-sample On/Off averages for left/right:
  `VR eye GPU [On, left]: scene/UI=... ms, resolve=... ms, final=... ms, total=... ms`.
  `total` is the sum of those intervals, not a new outer/nested query. Results
  are read only after every issued interval in that eye/slot is available;
  a busy slot skips the complete sample, never waiting for the GPU. Modes are
  captured at sample start so pending On measurements cannot become Off data.
- Scene/UI includes the multisampled world, stencil volumes and post-scene
  drawing. It does not isolate the cost of MSAA rasterization from the world;
  `resolve` measures the colour blit alone. Driver/query boundary overhead and
  CPU submission gaps mean these are diagnostic elapsed-GPU intervals, not a
  replacement for compositor timing or a controlled same-scene benchmark.

For the headset comparison, use one save and fixed render scale, inspect both a
high-contrast geometry edge and a baked-dark patch under the flashlight, and
record the left/right averages after warm-up. Toggle Off and On several times;
the package succeeds as an experiment only if the combined change is obvious
without unacceptable shimmer, crushed blacks, washed light colour or broken
stencil silhouettes.

### First complete-bundle headset result — 1 September 2026

The player reported a clear visual improvement, stronger depth/graphical
presence and fully black unlit caves on PSVR2, unlike the barely visible earlier
experiments. The flashlight alignment remained correct. This is a subjective
headset result; no matched OFF/ON image capture is retained in the repository.

The reviewed game log (build timestamp `Sep 1 2026 14:05:06`) reported an RTX
4070 Ti, OpenGL `4.6.0 NVIDIA 616.56`, active `RGBA16F`/MSAA 2× eye targets and
successful Cg compilation for the shader families actually encountered. No
enhanced-framebuffer fallback or shader compilation error was logged. Exit was
normal and the engine's memory tracker reported no remaining allocations.
Resource/model warnings were also present; without a matched baseline they
must not be called proven new rendering regressions or proven pre-existing bugs.

This run used render scale **1.50**, producing **5100 × 5202 per eye** (about
53 million stereo pixels before MSAA). The old complete-eye query logged:

| Mode | Left mean | Right mean | 120-sample windows per eye |
| --- | ---: | ---: | ---: |
| Off | 1.08 ms | 1.15 ms | 2 |
| On | 4.30 ms | 3.89 ms | 120 |

These unequal, moving-scene windows are **not** an isolated effect-cost A/B.
SteamVR targeted 90 Hz and reported 1825 reprojected of 19163 presents (about
9.5%); startup/timeouts are included, so this cannot all be attributed to the
bundle. The combined effect is not established as "almost free" at this scale.

The player clarified that the high scale was deliberate experimentation and
selected **1.00** for the next test. At the same recommended base resolution this
reduces pixel count by about 56%, not necessarily frame time by the same amount.

### Surface-refinement headset result — 1 September 2026, 21:30–21:37

The deployed executable SHA-256 matched the surface-refinement package
(`FCFD7CB5FA20737500E5D81E0011F094E9B448BF9991EC360C2B2C3750287F89`),
and its installed final shader still contained the original hard-cutoff curve.
This was not a report from an accidentally older executable. At **3400 × 3468
per eye**, the same RTX 4070 Ti/GL 4.6 driver loaded the encountered VR shaders
and enabled float/MSAA without a logged shader/FBO failure. Normal exit and the
engine's zero-allocation leak report were retained.

Mean logged times, in milliseconds; each window contains 120 eye samples:

| Mode / eye | Scene/UI | Resolve | Final | Total | Windows |
| --- | ---: | ---: | ---: | ---: | ---: |
| On / left | 1.415 | 0.238 | 0.210 | 1.863 | 235 |
| On / right | 1.191 | 0.619 | 0.178 | 1.988 | 235 |
| Off / left | 0.287 | 0 | 0 | 0.287 | 33 |
| Off / right | 0.279 | 0 | 0 | 0.279 | 33 |

The right-eye resolve is more variable than the left; elapsed queries can
include scheduling/resource waits. This log does not prove a specific blit bug
or justify adding `glFinish`/blocking synchronization. Moving scenes, map loads
and unequal On/Off windows still prevent treating the difference as isolated
effect cost. The 2560×1440 monitor view was also enabled; it is outside these
eye intervals.

SteamVR's matching session targeted **90 Hz**: 37824 presents, 58 dropped,
263 reprojected (**0.70%**, including startup/timeouts). It reported application
CPU/GPU averages of 4.219/1.938 ms and compositor GPU 0.330 ms. These measurement
boundaries differ from the eye queries and must not be added to them. HPL's
whole-session frame-rate average was 87.36. This run was substantially healthier
than the earlier high-scale run, but it is not a controlled benchmark of either
the resolution change or shader refinements alone.

The longer visual evaluation rejected the black level as excessive even in
illuminated areas. This prompted the grouped soft-toe v2 revision above, which
was subsequently rejected as too bright (see the later validation record).
No image-based A/B is claimed.

Other log findings, not silently "fixed" by graphics changes:

- Missing model-texture/joint/bone resources, plus one missing sound entity
  `unbreak_impact_fence_med.snt`; these require asset-specific investigation.
- One unavailable left skeleton summary near startup, followed by acquired
  left/right grip and aim poses. The reported `oculus_touch` emulation/binding
  identity is not changed based on this graphics test.
- Two standing-height calibration rejections at 0.820/0.774 m, below the existing
  0.90 m safety limit; no automatic recalibration or threshold relaxation.
- SteamVR `NvAPI Invalid refresh: 0` / present-wait errors begin at 21:38:02,
  **after** the game exited at 21:37:55, during the compositor shutdown period.
  They do not establish a game-renderer failure during this run.

## Held-flashlight lens alignment

The flashlight gap reported during the first hemispherical headset test was
unrelated to ambient shading. The held model used three incompatible forward
positions after its `VrScale = 1.6` transform:

- the physical front of the reconstructed mesh was approximately `0.1638 m`;
- the spotlight origin was at DAE `Y = -0.02`, while its flashlight-specific
  light entity kept `NearClipPlane = 0.1`; the scaled near plane consequently
  started at approximately `0.192 m`, leaving about `2.8 cm` beyond the lens;
- the front flare was at DAE `Y = -0.154435`, or approximately `0.247 m` after
  scaling, roughly `8.3 cm` in front of the physical lens.

The asset-only correction places the bulb at DAE `Y = -0.092`, shortens the
flashlight-specific near plane to `0.01`, and places the flare at the measured
lens plane `Y = -0.1024`. With the existing VR scale, the projected near plane
is approximately `0.1632 m`, within a millimetre of the reconstructed front.
The bulb remains about `1.7 cm` behind the lens, which gives the stencil shadows
a plausible physical source instead of placing it beyond the glass.

The alignment itself changes no renderer, gobo, falloff texture, direct-light
shader or stencil algorithm. `scripts/package.ps1` explicitly overlays the corrected
`light_player_flashlight_spot.lnt`, because the stock game otherwise supplies
that engine resource while the DAE already ships from `data/models`.

## Verification record

After removing SSAO and retaining the two-column VR Settings layout:

- Release Win32 full rebuild completed successfully on 31 August 2026.
- Large Address Aware was verified in the PE header.
- Unit tests reported `289 checks, 0 failures`.
- The packaged executable matched the built executable by SHA-256.
- No runtime code, shader or packaged filename matched the removed SSAO feature.

For the hemispherical ambient prototype on 1 September 2026:

- Release Win32 completed a full rebuild successfully.
- Large Address Aware remained present in the PE header.
- Project validation passed and unit tests reported `289 checks, 0 failures`.
- The package contains both optional shaders and its executable matches the
  built executable by SHA-256.
- The original ambient shaders have no source changes; the optional pair and
  runtime selection are guarded by the project validator.
- The first conservative headset A/B was completed and found correct but barely
  perceptible.

For the strong enhanced-ambient bundle and flashlight alignment on 1 September
2026:

- Release Win32 completed a clean full rebuild; after the final shader and
  documentation constants were selected, an incremental build confirmed that
  compiled sources were unchanged and regenerated the package.
- Large Address Aware remained present and unit tests again reported
  `289 checks, 0 failures`.
- Project validation guards the fixed hemisphere, direction, tint, bulb, lens,
  near-plane and packaging values.
- The package manifest contains the final optional shaders, corrected DAE,
  flashlight-specific `.lnt` overlay and renamed English/Spanish menu labels;
  their recorded SHA-256 values match the generated files.
- The strong ambient-only headset A/B was also judged too subtle. The corrected
  flare/beam origin was confirmed visually; this closes the alignment issue.

For the complete Enhanced visuals bundle on 1 September 2026:

- Source and package validation covers the original-Off path, auxiliary
  `RGBA16F`/MSAA colour-resolve path with retained depth/stencil, ten point/spot shader variants,
  fixed final-pass constants, alpha preservation, setting migration and all
  packaged shaders.
- The final Release Win32 full rebuild, Large Address Aware verification,
  project validation, `289 checks, 0 failures`, package generation and
  executable/resource hash verification all passed on 1 September 2026.
- The first complete-bundle headset A/B was positive and the runtime-loaded Cg
  families compiled successfully; the exact test conditions and performance
  limits are recorded above.

For the surface-refinement follow-up:

- `scripts/check-vr-shaders.ps1` compiles all 14 optional shaders using the
  bundled x86 Cg runtime and the tested `vp40`/`fp40` profiles, without starting
  the game or requiring an OpenGL context. This is now a build preflight.
- Offline assembly checks enforce unchanged texture-fetch counts, including
  exactly five for the final pass. Project checks guard relief/highlight
  constants, original-Off selection and nonblocking per-stage measurement.
- On 1 September 2026 the complete Release Win32 rebuild finished in 3m53s;
  Large Address Aware verification, project validation and all 289 unit checks
  passed. The package was regenerated with the revised shaders and documents.
- The subsequent headset run loaded the encountered programs and provided the
  split measurements above, but rejected excessive darkness in the retained
  tone curve. It did not supply a controlled visual or timing test of each
  individual surface refinement.

For soft-toe v2:

- Offline Cg checks confirm all 14 programs compile, four light variants lose
  one texture instruction each, and the final program drops from three scalar
  powers to one while retaining five texture instructions.
- `scripts/test-vr-visuals.ps1` adds 2231 CPU-reference checks tied to shader
  coefficients: black stays black, positive/monotonic dim-light response,
  finite output across the RGBA16F range, preserved RGB ratios, retained bright
  shoulder and no new sharpening extrema. This is a build preflight, not a
  substitute for shader execution or headset inspection.
- Release Win32 built successfully on 1 September 2026 using the safe incremental
  path (no header/project input changed). Large Address Aware, project checks,
  all 289 engine/VR unit checks and the shader/CPU preflights passed. The package
  includes the complete revised shader pairs, not just an updated executable.
- Runtime identification was `VR visual calibration: soft-toe v2`. Its next
  headset test rejected excessive brightness in illuminated rooms with held
  lights, plus an overly strong visibly layered glowstick halo. No isolated
  GPU savings are inferred from compilation.

For the tested dark-curve/ambient/held-light calibration (before v4):

- All 16 optional shaders compile with bundled Cg `vp40`/`fp40`; the two halo
  programs retain one/two texture instructions (without/with fog). The final
  pass retains five reads and restores three scalar powers.
- CPU references now report 8460 checks: restored tone, finite/monotonic neutral
  output, ambient-only display bounds, sharpening, halo profile, preserved
  alpha expressions and explicit VR/HUD gates. These do not execute C++ or GL.
- The nine-texture import verifies static opaque diffuse uses, unique resource
  basenames, dimensions, aspect and hashes; packaging rechecks the copies.
- The full Release Win32 rebuild completed in 3m46s on 1 September 2026; Large
  Address Aware, project checks and 289 engine/VR unit checks passed. All 16
  final shader versions were compiled offline, including the halo fog variant
  preserving original alpha multiplication order. Header layout changes were
  rebuilt consistently. Hardware appearance and GPU timing remain untested.
- The isolated installer fixture passed all 38 assertions under both PowerShell
  7 and Windows PowerShell 5.1; install, texture-only restore, re-enable and full
  restore were verified without changing the real installation.

Office-feedback evidence from the preceding dark-curve build (before v4):

- The tested installed executable matched SHA-256
  `AD0ED41BA6BA369494E9066CD27D4E6E372E13FE45261240C83ED7B665EF3E95`.
  The log closed at 22:39 on 1 September 2026, confirmed dark-curve activation,
  both halo programs and a successful exit. Asset warnings remain, but no
  new shader compilation or selected-texture failure was identified.
- In the office section, 33 reported On windows averaged about 1.93 ms left /
  2.03 ms right; three Off windows about 0.49 / 0.49 ms. Final pass averaged
  0.20 / 0.16 ms On; resolve about 0.21 / 0.58 ms, with right-eye variability.
  Positions, light state and windows are not matched: these are session
  observations, not isolated effect costs or a v4 benchmark. Cross-map pending
  query windows can further blur section boundaries.

For the office-feedback v4 revision:

- All 16 v4 programs compile offline with unchanged texture-fetch counts.
  CPU visual checks now total 8752, including capped ambient delta, restrained
  recovery across all ten programs and removal of the six specular boosts.
  Release Win32 completed via the safe incremental path (unchanged headers);
  Large Address Aware, 289 engine/VR checks, texture checks and packaging passed.
- The subsequent user report was positive ("está mejor"). Preserve this
  calibration as the next comparison baseline. No matched v4 captures or
  controlled per-eye GPU benchmark accompany that report, and it does not
  separately validate every halo/fog condition or the full texture selection.
- The 2 September follow-up tested all 231 installed images across multiple
  maps with positive feedback and a clean shutdown. This calibration is now
  frozen for `v0.1.0`; [validation](VALIDATION-v0.1.0.md) records the actual
  coverage, 1.07% session reprojection, asymmetric resolve timings and absence
  of a matched On/Off or peak-memory benchmark. Follow-up work is tracked in
  [ROADMAP](ROADMAP.md#post-stable-follow-up).

Relevant code entry points:

- `HPL1Engine/sources/graphics/Renderer3D.cpp` — eye FBO creation and render-pass
  order.
- `HPL1Engine/sources/graphics/Material_BaseLight.cpp` — ambient and enhanced
  point/spot program selection plus the global/sector ambient uniform.
- `HPL1Engine/assets/core/programs/Diffuse_Color_vp.cg` — current ambient vertex
  path, which does not consume normals.
- `HPL1Engine/assets/core/programs/Ambient_Color_fp.cg` — current diffuse-times-
  ambient fragment path.
- `HPL1Engine/assets/core/programs/VR_Enhanced_Final_fp.cg` — fixed sharpening,
  exposure, tone, colour, contrast and gamma shaping.
- `HPL1Engine/sources/game/ScriptFuncs.cpp` — map-script ambient adjustments.
- `HPL1Engine/sources/scene/Scene.cpp` — VR-eye and monitor render boundaries.
