"""diagnose_anatomy2.py - Point 1 (refined): per-finger anatomy along the REAL
longitudinal axis (projection of vertices onto the J1-J2-J3 polyline), BOTH hands.

For each finger:
  - Joint1/2/3 positions and their arc-length coordinate s on the axis
  - visual start/end of each of the 3 mesh segments (min/max s of the verts
    assigned to that bone), also in x
  - geometric segment lengths (s-extents) vs joint distances d12/d23
  - visual articulation seams (midpoint between consecutive segments) vs Joint2/Joint3
  - free-tube start (from the neighbour-separation analysis)

Read-only.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
LEFT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae'

# free-tube start x (both hands identical): from diagnose_anatomy.py
FREE_START = {'Little': 13.5, 'Ring': None, 'Middle': None, 'Index': 18.5, 'Thumb': 9.0}


def polyline_s(pts):
    ds = [np.linalg.norm(pts[i + 1] - pts[i]) for i in range(len(pts) - 1)]
    s = [0.0]
    for d in ds:
        s.append(s[-1] + d)
    return s


def project(p, pts, slen):
    best = (1e18, -1, 0.0, 0.0)
    for seg in range(len(pts) - 1):
        a, b = pts[seg], pts[seg + 1]
        ab = b - a
        L2 = ab @ ab
        t = ((p - a) @ ab) / L2 if L2 > 0 else 0.0
        tc = min(max(t, 0.0), 1.0)
        q = a + tc * ab
        d = np.linalg.norm(p - q)
        s = slen[seg] + tc * (slen[seg + 1] - slen[seg])
        if d < best[0]:
            best = (d, seg, tc, s)
    return best


def analyze(path, label):
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(path)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)
    print('\n' + '=' * 104)
    print('== %s' % label)
    print('=' * 104)

    for cname, bones in fw.CHAINS:
        jidx = [idx[b] for b in bones]
        pts = [joint_pos[j] for j in jidx]
        slen = polyline_s(pts)
        # project every vertex onto this finger's axis
        proj = np.array([project(p, pts, slen) for p in pos])  # (d, seg, t, s)
        S = proj[:, 3]

        print('\n---- %s (axis J1->J2->J3, arc length s) ----' % cname)
        print('  joints: J1 s=0.00 x=%.2f | J2 s=%.2f x=%.2f | J3 s=%.2f x=%.2f' %
              (pts[0][0], slen[1], pts[1][0], slen[2], pts[2][0]))
        print('  d12 = %.2f   d23 = %.2f   total = %.2f' % (slen[1], slen[2] - slen[1], slen[2]))

        seg = {}
        for k, b in enumerate(bones):
            m = np.where(new_bone == jidx[k])[0]
            s_ = S[m]
            x_ = pos[m][:, 0]
            seg[k] = (s_.min(), s_.max(), x_.min(), x_.max())
            print('  segment %d (%s): s [%.2f .. %.2f]  x [%.2f .. %.2f]  len(s) %.2f' %
                  (k + 1, b, s_.min(), s_.max(), x_.min(), x_.max(), s_.max() - s_.min()))

        # visual articulation seams
        a1 = (seg[0][1] + seg[1][0]) / 2.0
        a2 = (seg[1][1] + seg[2][0]) / 2.0
        print('  visual articulation seams: A1 (seg1|seg2) at s=%.2f  A2 (seg2|seg3) at s=%.2f' % (a1, a2))
        print('  Joint2 vs A1: |s(J2)-A1| = %.2f   Joint3 vs A2: |s(J3)-A2| = %.2f' %
              (abs(slen[1] - a1), abs(slen[2] - a2)))
        d1 = abs(slen[1] - a1)
        d2 = abs(slen[2] - a2)
        print('  verdict: Joint2 %s | Joint3 %s' %
              ('OK' if d1 < 0.5 else 'SHIFTED +%.2f' % (a1 - slen[1]),
               'OK' if d2 < 0.5 else 'SHIFTED +%.2f' % (a2 - slen[2])))
        # segment geometric length vs joint span
        print('  geom seg1 len %.2f vs d12 %.2f (delta %+.2f) | geom seg2 len %.2f vs d23 %.2f (delta %+.2f) | seg3 len %.2f' %
              (seg[0][1] - seg[0][0], slen[1], (seg[0][1] - seg[0][0]) - slen[1],
               seg[1][1] - seg[1][0], slen[2] - slen[1], (seg[1][1] - seg[1][0]) - (slen[2] - slen[1]),
               seg[2][1] - seg[2][0]))
        fs = FREE_START[cname]
        if fs is not None:
            # s coordinate of the free start (approx via x along the axis)
            print('  free-tube start: x=%.1f (%.1f inside segment 1, i.e. s=%.2f, J1 at s=0)' %
                  (fs, fs - pts[0][0], fs - pts[0][0]))
        else:
            print('  free-tube start: NEVER (fused with neighbour to the tip)')


analyze(RIGHT, 'RIGHT HAND')
analyze(LEFT, 'LEFT HAND')