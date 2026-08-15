"""diagnose_anatomy.py - Point 2: per-finger geometry anatomy, BOTH hands.

For each finger (Little/Ring/Middle/Index/Thumb):
  - Joint1/2/3 mesh positions + longitudinal axis
  - where the visual finger geometry starts (tube separation from neighbours)
  - mesh segment extents (weight regions) and joint distances
  - mismatches: visual base/middle/tip vs joint positions

Read-only.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
LEFT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae'


def analyze(path, label):
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(path)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)
    print('\n' + '=' * 100)
    print('== %s' % label)
    print('=' * 100)

    # chain axes
    chain = {}
    for cname, bones in fw.CHAINS:
        jidx = [idx[b] for b in bones]
        pts = [joint_pos[j] for j in jidx]
        axis = pts[1] - pts[0]
        L = np.linalg.norm(axis)
        chain[cname] = (bones, jidx, pts, axis / L, L, radii[cname])

    # per-x-slice: z-extent of each chain's tube (single pass)
    xs = np.arange(9.0, 22.0, 0.5)
    extent = {c: {} for c in chain}
    for x in xs:
        mask = np.abs(pos[:, 0] - x) < 0.25
        for cname, (bones, jidx, pts, dirv, L, R) in chain.items():
            q = pts[1] + dirv * (((pos[mask] - pts[1]) @ dirv)[:, None] * dirv)
            d = np.linalg.norm(pos[mask] - q, axis=1)
            sel = np.where(d < R + 0.6)[0]
            if len(sel):
                zs = pos[mask][:, 2][sel]
                extent[cname][x] = (zs.min(), zs.max())

    # adjacent pairs by z order: Little-Ring, Ring-Middle, Middle-Index, Index-Thumb
    zorder = [c for c in chain]
    zorder.sort(key=lambda c: chain[c][2][0][2])  # by J1 z
    pairs = list(zip(zorder, zorder[1:]))
    pair_sep = {}
    for a, b in pairs:
        for x in xs:
            ea = extent[a].get(x)
            eb = extent[b].get(x)
            if ea is None or eb is None:
                continue
            # z-disjoint (no overlap) -> separated
            if ea[1] < eb[0] or eb[1] < ea[0]:
                pair_sep[(a, b)] = x
                break
        if (a, b) not in pair_sep:
            pair_sep[(a, b)] = 21.0  # never separates
    print('  adjacent-pair separation x: %s' %
          ', '.join('%s-%s@%.1f' % (a, b, v) for (a, b), v in pair_sep.items()))

    for cname, (bones, jidx, pts, dirv, L, R) in chain.items():
        print('\n---- %s ----' % cname)
        for b, j in zip(bones, jidx):
            print('  %-8s (%8.3f, %8.3f, %8.3f)' % (b, *joint_pos[j]))

        sides = [pair_sep[(a, b)] for a, b in pairs if a == cname or b == cname]
        vis_base = max(sides) if sides else 21.0
        print('  separation from each side: %s' %
              ', '.join('%.1f' % s for s in sides) or '-')
        print('  visual finger base (both sides free): x=%.1f' % vis_base)
        print('  Joint1 at x=%.2f  -> visual base mismatch = %+.1f' % (pts[0][0], vis_base - pts[0][0]))

        # mesh segment extents (weight regions)
        for k, b in enumerate(bones):
            sel = np.where(new_bone == jidx[k])[0]
            if len(sel):
                xr = pos[sel][:, 0]
                print('  region %-8s x %.2f..%.2f  (len %.2f)' % (b, xr.min(), xr.max(), xr.max() - xr.min()))

        d12 = np.linalg.norm(pts[1] - pts[0])
        d23 = np.linalg.norm(pts[2] - pts[1])
        sel3 = np.where(new_bone == jidx[2])[0]
        tipx = pos[sel3][:, 0].max()
        print('  Joint1->Joint2 = %.2f   Joint2->Joint3 = %.2f   Joint3->tip = %.2f (tip x=%.2f)' %
              (d12, d23, tipx - pts[2][0], tipx))

        segs = []
        for k, b in enumerate(bones):
            sel = np.where(new_bone == jidx[k])[0]
            xr = pos[sel][:, 0]
            segs.append(xr.max() - xr.min())
        print('  visual mesh segments (x-extents): base %.2f  mid %.2f  distal %.2f' % tuple(segs))

        sel1 = np.where(new_bone == jidx[1])[0]
        mid_vis = pos[sel1][:, 0].mean()
        print('  mismatch: visual middle center %.2f vs Joint2 x=%.2f (%+.1f)   tip %.2f vs Joint3 x=%.2f (%+.1f)' %
              (mid_vis, pts[1][0], mid_vis - pts[1][0], tipx, pts[2][0], tipx - pts[2][0]))


analyze(RIGHT, 'RIGHT HAND')
analyze(LEFT, 'LEFT HAND')