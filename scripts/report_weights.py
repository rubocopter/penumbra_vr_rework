"""Dry-run weight report: BEFORE (original backup) vs AFTER (reassignment).

No files are written. For every finger chain (Little/Ring/Middle/Index/Thumb)
the report splits the mesh by projection intervals on the joint polyline
(X1->X2->X3, same rule as fix_rig_weights.assign):

  base   : nearest point on segment X1->X2   -> expected bone X1
  middle : nearest point on segment X2->X3   -> expected bone X2
  tip    : projection beyond X3              -> expected bone X3
  palm   : x >= 9 and not within R of any chain -> expected bone Palm
  sleeve : x < 9                             -> expected bone Hand_Root

Each row: vertex count, BEFORE top bones (%), AFTER top bones (%), spatial
range (bounding box) of the vertices in the region.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

BACKUP = {
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae':
        'E:/penumbra_vr-master/.penumbravr/backup/hud_object_hand_rig.dae.pre-weightfix',
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae':
        'E:/penumbra_vr-master/.penumbravr/backup/hud_object_hand_left_rig.dae.pre-weightfix',
}


def bone_of_vtx(old_v, old_vc, i):
    off = int(old_vc[:i].sum())
    return int(old_v[2 * off])


def top_bones(old_v, old_vc, sel, joints):
    d = {}
    for i in sel:
        b = bone_of_vtx(old_v, old_vc, i)
        d[b] = d.get(b, 0) + 1
    n = len(sel)
    items = sorted(d.items(), key=lambda kv: -kv[1])
    return ', '.join(f'{joints[b]} {c*100//n}%' for b, c in items[:4])


def bbox(pos, sel):
    if not sel:
        return 'n/a'
    p = pos[sel]
    lo = p.min(axis=0)
    hi = p.max(axis=0)
    return (f'x[{lo[0]:.1f}..{hi[0]:.1f}] y[{lo[1]:.1f}..{hi[1]:.1f}] '
            f'z[{lo[2]:.1f}..{hi[2]:.1f}]')


def segment_regions(pos, joints, joint_pos, radii, cname, bones):
    """Returns {'base': [idx], 'middle': [idx], 'tip': [idx]} for one chain."""
    idx = {n: i for i, n in enumerate(joints)}
    jidx = [idx[b] for b in bones]
    pts = [joint_pos[j] for j in jidx]
    out = {'base': [], 'middle': [], 'tip': []}
    for i, p in enumerate(pos):
        if p[0] < fw.SLEEVE_X:
            continue
        d, seg = fw.nearest_on_polyline(p, pts)
        if d <= radii[cname]:
            if seg == 0:
                out['base'].append(i)
            elif seg == 1:
                out['middle'].append(i)
            else:
                out['tip'].append(i)
    return out


def run(path, label):
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(path)
    idx = {n: i for i, n in enumerate(joints)}
    print(f'==== {label} ({path.split("/")[-1]}) ====')
    radii = fw.measure_radii(pos, joints, joint_pos)

    orig = fw.parse(BACKUP[path])
    ov, ovc = orig[9], orig[8]

    new_bone = fw.assign(pos, joints, joint_pos, radii)

    # consistency: computed AFTER must equal what is already in the data file
    cur = np.array([int(v[2 * i]) for i in range(len(new_bone))])
    print('consistent with applied weights:', bool((cur == new_bone).all()))

    # sleeve
    sel = [i for i in range(len(pos)) if pos[i][0] < fw.SLEEVE_X]
    print(f'\nSleeve (x<9)  n={len(sel):4d}  {bbox(pos, sel)}')
    print(f'  BEFORE: {top_bones(ov, ovc, sel, joints)}')
    print(f'  AFTER:  {top_bones(v, vc, sel, joints)}')

    # palm: x>=9 not captured by any chain
    sel = [i for i in range(len(pos)) if new_bone[i] == idx['Palm']]
    print(f'\nPalm  n={len(sel):4d}  {bbox(pos, sel)}')
    print(f'  BEFORE: {top_bones(ov, ovc, sel, joints)}')
    print(f'  AFTER:  {top_bones(v, vc, sel, joints)}')

    for cname, bones in fw.CHAINS:
        regs = segment_regions(pos, joints, joint_pos, radii, cname, bones)
        print(f'\n{cname}  (joints at x={joint_pos[idx[bones[0]]][0]:.1f}, '
              f'{joint_pos[idx[bones[1]]][0]:.1f}, {joint_pos[idx[bones[2]]][0]:.1f})')
        for segname, eb in (('base', 0), ('middle', 1), ('tip', 2)):
            s = regs[segname]
            print(f'  {segname:6s} n={len(s):4d}  {bbox(pos, s)}')
            print(f'    BEFORE: {top_bones(ov, ovc, s, joints)}')
            print(f'    AFTER:  {top_bones(v, vc, s, joints)}')
            want = bones[eb]
            bad = [i for i in s if new_bone[i] != idx[want]]
            print(f'    expected {want}: {"OK (100%)" if not bad else f"MISMATCH {len(bad)}"}')


for path, label in (('E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae', 'RIGHT'),
                    ('E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae', 'LEFT')):
    run(path, label)