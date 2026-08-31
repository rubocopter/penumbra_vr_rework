# Rendering and lighting research

This document records the lighting investigation performed against the current
HPL1/OpenVR renderer, including the discarded SSAO prototype and the smallest
remaining candidate for improving surface volume. It separates verified current
behaviour from experiments and future work so that a prototype cannot silently
become a shipping renderer dependency.

## Current status

- The shipping renderer has no SSAO pass, depth sampling, contact-shadow pass,
  HDR path, or ray-traced lighting.
- Each VR eye uses an `RGBA8` colour texture and a packed
  `GL_DEPTH24_STENCIL8` renderbuffer. The original stencil shadow-volume path is
  intact.
- The monitor renderer and all direct-light/material shaders are unchanged.
- The SSAO feasibility prototype was removed after hardware evaluation showed
  too little visible benefit.
- Hemispherical ambient lighting is a design candidate only. It has not been
  implemented and no setting or shader for it currently exists.

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

`CreateFrameBuffer()` creates two independent eye targets. Each contains:

- one sampleable `GL_RGBA8` colour texture attached to
  `GL_COLOR_ATTACHMENT0`;
- one `GL_DEPTH24_STENCIL8` renderbuffer attached to the combined
  `GL_DEPTH_STENCIL_ATTACHMENT`.

The framebuffer is accepted only when `glCheckFramebufferStatus()` returns
`GL_FRAMEBUFFER_COMPLETE`; oversized eye buffers are retried at a smaller size.
HPL1 accesses the OpenGL framebuffer entry points through GLee. Nothing in the
shadow-volume algorithm requires the attachment to be a renderbuffer: it
requires a working stencil attachment. Nevertheless, the current renderer
deliberately uses the original renderbuffer because no shipping effect samples
depth.

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
| Hemispherical ambient | Next candidate | Uses existing geometry normals inside the ambient pass; no depth texture or extra pass |
| Soft ambient direction | Possible later extension | Could increase separation, but should not be mixed into the first controlled test |
| Separate sky/ground colours | Possible later extension | More art direction and tuning than the initial scalar A/B needs |
| Fake contact shading | Low priority | The SSAO test indicates that more contact darkening is not the main visual deficit |
| Distance attenuation/fog | Deferred | Potentially useful for depth separation but much more likely to change Penumbra's original atmosphere |
| HDR/modern post-processing | Out of scope | The current PCVR/OpenGL path is `RGBA8`, and converting the presentation pipeline is not a minimal lighting improvement |

## Proposed hemispherical ambient prototype

The first test should alter only the orientation of the existing ambient term:

```text
hemi = saturate(normalWorld.y * 0.5 + 0.5)
ambientFactor = 0.75 + (1.0 - 0.75) * hemi
output.rgb = diffuse.rgb * globalAmbient * sectorAmbient * ambientFactor
output.a = diffuse.a
```

`Up = 1.0` and `Down = 0.75` are deliberately conservative. An upward-facing
surface keeps the current ambient level, a downward-facing surface receives
75 per cent, and a vertical surface lands halfway between them. The aim is to
test whether orientation alone separates floors, walls, ceilings and props—not
to introduce an obvious new light source.

### Minimal implementation seam

1. Leave the shared `Diffuse_Color_vp.cg` and `Ambient_Color_fp.cg` untouched.
   They are reused by paths that must remain identical.
2. Add a dedicated ambient vertex/fragment pair for the BaseLight Z pass.
3. Feed the geometry normal through an inverse-transpose object-to-world normal
   matrix and use world Y. View-space Y would make the lighting rotate with the
   player's head and differ conceptually between eye views.
4. Select the alternate pair only when the low-level renderer reports VR active
   and a future runtime toggle is On. A shader creation failure must fall back
   to the original pair.
5. Keep `RenderLight()`, materials' direct-light programs, stencil operations,
   eye FBOs and the monitor path untouched.

HPL1 already exposes vertex normals and per-object program setup hooks;
`Material_EnvMap_Reflect.cpp` demonstrates sending `objectWorldMatrix` during a
draw. Computing and interpolating the hemispherical factor in the vertex shader
keeps the fragment work to a small interpolation/multiply. Passing an
inverse-transpose normal matrix is safer than taking the object's upper-left
3×3 directly because non-uniform scale can otherwise skew the result.

The first prototype should cover the normal BaseLight material family. Legacy
fallback or unusual material paths should retain the original ambient shader
rather than receive a broad modification to a shared program.

### Required A/B contract

- `VR toggle Off`: select the exact original ambient programs, not a factor of
  one inside the experimental shader.
- `VR toggle On`: apply only the fixed `Up = 1.0`, `Down = 0.75` orientation
  factor.
- Monitor rendering: always select the original programs.
- Runtime change: immediate, with no map reload or restart.
- Default during evaluation: Off.
- Alpha, direct lights, flashlight stencil shadows, fog and materials: unchanged.
- No intensity, direction, colour, radius or quality controls in the first test.

### Evaluation protocol

Use the same save, camera position, head orientation, render scale and active
lights for each pair. Capture the same eye with the toggle Off and On after a
short warm-up. Inspect at least a floor/wall/ceiling junction, a freestanding
prop, a downward-facing surface and a flashlight shadow edge. Record independent
GPU time for left and right eyes over enough frames to report a stable median,
not a single-frame sample.

The prototype succeeds only if surface orientation gives perceptible volume
without looking like a new directional light. If the A/B remains irrelevant,
remove it as completely as SSAO. If it succeeds, tuning and optional sky/ground
colour can be considered in a separate change.

## Verification record

After removing SSAO and retaining the two-column VR Settings layout:

- Release Win32 full rebuild completed successfully on 31 August 2026.
- Large Address Aware was verified in the PE header.
- Unit tests reported `289 checks, 0 failures`.
- The packaged executable matched the built executable by SHA-256.
- No runtime code, shader or packaged filename matched the removed SSAO feature.

Relevant code entry points:

- `HPL1Engine/sources/graphics/Renderer3D.cpp` — eye FBO creation and render-pass
  order.
- `HPL1Engine/sources/graphics/Material_BaseLight.cpp` — ambient program
  selection and global/sector uniform.
- `HPL1Engine/assets/core/programs/Diffuse_Color_vp.cg` — current ambient vertex
  path, which does not consume normals.
- `HPL1Engine/assets/core/programs/Ambient_Color_fp.cg` — current diffuse-times-
  ambient fragment path.
- `HPL1Engine/sources/game/ScriptFuncs.cpp` — map-script ambient adjustments.
- `HPL1Engine/sources/scene/Scene.cpp` — VR-eye and monitor render boundaries.
