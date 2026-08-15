"""diagnose_rotate.py - Point 3: single-joint 10-degree rotation test.

For each joint (Index1 first, then Middle1/2/3, then Ring1/2):
  ENGINE behavior (actual game): the joint's local rotation is applied at the
  ORIGIN because the engine reads the bind translation as (0,0,0)
  (transposed bind matrices).
  REFERENCE behavior (corrected bind): the rotation is applied at the joint's
  mesh position (pivot at the joint).

Per test we report: region, pivot, direction of motion, base/middle/tip
positions before/after, and whether the segments stay joined.

Read-only.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'

ANGLE = 10.0  # degrees
AXIS = np.array([0.0, 0.0, -1.0])  # PlayerHands.cpp finger axis (right hand)


def rot_matrix(axis, deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    k = axis / np.linalg.norm(axis)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + s * K + (1 - c) * (K @ K)


def main():
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(RIGHT)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)

    R = rot_matrix(AXIS, ANGLE)

    tests = [
        ('Index1', ['Index1', 'Index2', 'Index3']),
        ('Middle1', ['Middle1', 'Middle2', 'Middle3']),
        ('Middle2', ['Middle1', 'Middle2', 'Middle3']),
        ('Middle3', ['Middle1', 'Middle2', 'Middle3']),
        ('Ring1', ['Ring1', 'Ring2', 'Ring3']),
        ('Ring2', ['Ring1', 'Ring2', 'Ring3']),
    ]

    for jname, chain in tests:
        j = idx[jname]
        J = joint_pos[j]
        chain_idx = [idx[b] for b in chain]
        verts = np.where(np.isin(new_bone, chain_idx))[0]
        seg1 = np.where(new_bone == idx[chain[0]])[0]
        seg2 = np.where(new_bone == idx[chain[1]])[0]
        seg3 = np.where(new_bone == idx[chain[2]])[0]
        base = joint_pos[idx[chain[0]]]
        mid = joint_pos[idx[chain[1]]]
        tip = joint_pos[idx[chain[2]]]

        # ENGINE: rotate about origin
        p_eng = (R @ pos[verts].T).T
        # REFERENCE: rotate about joint
        p_ref = (R @ (pos[verts] - J).T).T + J

        # fold angle: angle between segment vectors after deformation
        def fold(P):
            # segment directions from the joint's child geometry (mesh positions)
            d1 = mid - base
            d2 = tip - mid
            return np.degrees(np.arccos(np.clip(d1 @ d2 / (np.linalg.norm(d1) * np.linalg.norm(d2)), -1, 1)))

        # engine: the whole chain rotates rigidly -> the fold angle of the CHAIN
        # (geometry) is unchanged; the chain axis tilts by ANGLE about the origin.
        base_eng = R @ base
        mid_eng = R @ mid
        tip_eng = R @ tip
        # reference: base stays, mid/tip rotate about J
        mid_ref = R @ (mid - J) + J
        tip_ref = R @ (tip - J) + J

        # direction of motion (unit displacement)
        def udir(p0, p1):
            d = p1 - p0
            return d / (np.linalg.norm(d) + 1e-12)

        # seam continuity: max gap between parent segment boundary verts and
        # child boundary verts (the two rings at the joint)
        def seam_gap(piv, pa, pb):
            # parent ring = verts of segment k nearest the joint, child ring = of k+1
            pa = np.asarray(pa, dtype=float)
            pb = np.asarray(pb, dtype=float)
            dmin = np.array([np.min(np.linalg.norm(pb - q, axis=1)) for q in pa])
            return dmin.max()

        g_before = seam_gap(J, pos[seg1], pos[seg2])
        g_after_eng = seam_gap(J, (R @ pos[seg1].T).T, (R @ pos[seg2].T).T)
        g_after_ref = seam_gap(J, pos[seg1], (R @ (pos[seg2] - J).T).T + J)

        print('\n---- single-joint 10 deg test: %s (chain %s) ----' % (jname, '-'.join(chain)))
        print('  joint mesh position: (%.2f, %.2f, %.2f)   axis (0,0,-1)   angle 10 deg' % tuple(J))
        print('  ENGINE (actual): pivot = ORIGIN (0,0,0)')
        print('    base    (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.2f  dir %s' %
              (*(base), *(base_eng), np.linalg.norm(base_eng - base), udir(base, base_eng).round(2)))
        print('    middle  (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.2f  dir %s' %
              (*(mid), *(mid_eng), np.linalg.norm(mid_eng - mid), udir(mid, mid_eng).round(2)))
        print('    tip     (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.2f  dir %s' %
              (*(tip), *(tip_eng), np.linalg.norm(tip_eng - tip), udir(tip, tip_eng).round(2)))
        print('    fold angle at %s: 0.0 deg (segments rigid, chain tilts about wrist)' % jname)
        print('    chain displacement: %d verts move  |d|max=%.2f  (rigid body)' %
              (len(verts), np.linalg.norm(p_eng - pos[verts], axis=1).max()))
        print('  REFERENCE (corrected bind): pivot = JOINT (%.2f, %.2f, %.2f)' % tuple(J))
        print('    base    stays (|d|=0.00)')
        print('    middle  (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.2f  dir %s' %
              (*(mid), *(mid_ref), np.linalg.norm(mid_ref - mid), udir(mid, mid_ref).round(2)))
        print('    tip     (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.2f  dir %s' %
              (*(tip), *(tip_ref), np.linalg.norm(tip_ref - tip), udir(tip, tip_ref).round(2)))
        print('    fold angle at %s: %.1f deg (segments %s..%s bend at the joint)' %
              (jname, np.degrees(np.arccos(np.clip(
                  (mid - base) @ (tip_ref - mid) / (np.linalg.norm(mid - base) * np.linalg.norm(tip_ref - mid)), -1, 1))),
               chain[0], chain[1]))
        print('    segments stay joined: seam gap before %.3f -> after %.3f (pivot=joint)' %
              (g_before, g_after_ref))
        print('  seam continuity (engine): %.3f (rigid -> same as before %.3f)' % (g_after_eng, g_before))
        # segment count per direction for engine: which verts move the most
        dmax = np.linalg.norm(p_eng - pos[verts], axis=1)
        i = int(np.argmax(dmax))
        print('  max-mover vert (engine): (%.2f, %.2f, %.2f) bone=%s  |d|=%.2f' %
              (*(pos[verts][i]), joints[new_bone[verts[i]]], dmax[i]))


main()