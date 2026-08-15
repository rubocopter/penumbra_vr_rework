# DIAGNOSIS: VR hand deformation — root cause found (2026-08-15)

Verdict: **Hypothesis B — the bind matrices in the .dae files are TRANSPOSED
relative to the engine's expected layout. The skeleton collapses to the
origin; every finger rotation pivots at the wrist.**

## Evidence chain (Tests 1-5)

### Test 4 — bind / inv_bind layout (ROOT CAUSE)
- HPL1 `cMatrixf(const double* pA)` (`include/math/Matrix.h:57-75`) reads the
  16 floats ROW-MAJOR: `m[row][col] = pA[row*4+col]`.
- `cMatrixf::GetTranslation()` returns `(m[0][3], m[1][3], m[2][3])`
  (`Matrix.h:236-240`) -> the translation is read from **floats 3, 7, 11**
  (column 3).
- The controller's INV_BIND_MATRIX source is loaded via that constructor
  (`MeshLoaderColladaHelpers.cpp:1070`: `cMatrixf mtxTemp(&vValVec[i*16])`).
- The mod's `hud_object_hand_rig.dae` bind_poses matrices put the translation
  in **floats 12..14** (last row), e.g. Little1:
  `1 0 0 0 / 0 1 0 0 / 0 0 1 0 / -9.159 -2.225 5.984 1`
  -> the engine reads translation (0,0,0) for EVERY bone.
- The ORIGINAL game's working skinned model (`character_dog_base.dae`,
  skinCluster1-Matrices) uses the standard layout: translation at floats
  3/7/11, last row `0 0 0 1`. The dog animates correctly in-game -> the
  engine is correct; the mod's FBX export is wrong.
- Both hand files (right + left) are affected identically.

### Consequence in the engine
- `MeshLoaderCollada.cpp:333`:
  `pBone->SetTransform(cMath::MatrixInverse(Ctrl.mvMatrices[j]))`
  -> bone BIND WORLD = inverse(identity) = **identity** for all 17 bones
  (the scene-node translations from `CreateSkeletonBone`
  `MeshLoaderColladaHelpers.cpp:485-498` are OVERWRITTEN).
- `CalcLocalMatrixRec` (`MeshLoaderColladaHelpers.cpp:455-480`) -> all local
  matrices = identity.
- Runtime (PlayerHands.cpp:164-238 builds rotation-only tracks, applied via
  the standard animation slerp on the bone LOCAL rotation):
  bone world = parent(identity) * R_anim = R_anim ->
  **every finger rotates about the ORIGIN (the wrist)**, not about the joint.
- Vertices: skin = R_anim * invBindWorld(identity) -> the whole finger chain
  swings as rigid rods around the wrist.

### Explains every symptom
- "Palma estable": Palm and Hand_Root tracks rotate 0 deg -> Palm-weighted
  verts never move.
- "Dedos tubo rígido / ángulos artificiales / falanges separadas / mano
  esquelética": at grip=1 the little/ring/middle accumulate ~150-165 deg of
  rotation about the wrist -> tips fly past vertical, behind the hand.
- The deformation existed since the mod's FIRST version (the user's original
  complaint) - the weight fix (previous phase) was necessary but NOT
  sufficient: the skeleton was never at the joints.
- verify_skin.py (joint pivots, grip=1): little base dy +0.90, middle tip
  dy +2.37, sleeve 0.0000 = the CORRECT behavior that the fix restores.

### Tests 1/2/3/5 — all consistent, no other defect
- Test 5 (hierarchy): scene nodes correct (pure translates, match bind),
  `Hand_Root->Palm->{5 chains X1->X2->X3}`, no surprises; overwritten by the
  controller pass (the bug above).
- Test 1 (mesh): joints lie exactly on the tube axes (err 0.000000 when the
  matrices are read in the standard layout).
- Test 2 (single-joint): rigid rotation math verified, pivot error 0.000000
  with standard layout.
- Test 3 (chain lengths): Little 5.40+3.63+0.78, Ring 6.62+4.45+0.96, Middle
  6.75+4.54+0.98, Index 5.10+3.43+0.74, Thumb 2.09+1.41+0.30 - plausible for
  the mesh; not a defect.
- Hypotheses A (bind pose), C (skinning math), D (ApplyToNode): ruled out by
  the code path above and the previous session's verification.

## Proposed fix (ONE change, pending approval)
Transpose the 17 bind matrices per hand file: move each matrix's translation
from floats 12..14 to floats 3, 7, 11 (the layout the engine reads, identical
to the dog's working files). E.g. Little1:

    before: 1 0 0 0  0 1 0 0  0 0 1 0  -9.159 -2.225 5.984  1
    after:  1 0 0 -9.159  0 1 0 -2.225  0 0 1 5.984  0 0 0 1

- Then bind world = inverse = T(+J): pivots land exactly on the joints
  (error 0.000000, verified in diagnose_anatomy.py).
- NO changes to weights, geometry, axes, angles, animation, PlayerHands.cpp,
  SteamVR, tracking or haptics.
- Applies to both `hud_object_hand_rig.dae` and `hud_object_hand_left_rig.dae`.

## Files
- scripts/diagnose_anatomy.py   (Tests 1-3, single-joint, separation table)
- scripts/diagnose_grip.py      (grip=1 simulation: folds, edge stretch)
- scripts/render_hand.py        (render_0/1/2.png: bind vs grip, per-bone color)
- scripts/fix_rig_weights.py    (previous phase: weight correction, keep)