"""Reassign skin weights in the VR hand rig .dae files.

The original rig assigns each finger tube mostly to its DISTAL bones (X2/X3),
so the knuckle bones (X1) have ~0% influence and the tubes bend around the
wrong pivots (deformed/skeletal hand). This script reassigns every vertex to
the bone of the tube segment it lies on:

  - sleeve (x < 9)              -> Hand_Root
  - segment knuckle->mid (X1X2) -> X1  (rotates around the knuckle)
  - segment mid->tip (X2X3)     -> X2  (rotates around the mid joint)
  - beyond the tip joint (X3)   -> X3
  - everything else (palm)      -> Palm

Selection: nearest point on each finger's joint polyline (X1->X2->X3); if the
distance is <= radius (measured from the mesh, p50 of the mid-proximal tube
cross-section + 0.45) the vertex belongs to that tube, otherwise it is palm.
Hard 100% assignment per vertex (same style as the original rig).

IMPORTANT: the written file keeps the COLLADA namespace as the DEFAULT
namespace (xmlns="..."), exactly like the original exporter output. HPL1's
loader (TinyXML, MeshLoaderColladaLoader.cpp) matches elements by literal
name ("asset", "library_geometries", ...), and ElementTree would otherwise
serialize the namespace as a "ns0:" prefix, making every lookup fail
(hands silently invisible).

Usage:
  python fix_rig_weights.py --dry-run
  python fix_rig_weights.py --apply
"""

import argparse
import sys
import xml.etree.ElementTree as ET

import numpy as np

NS = {'c': 'http://www.collada.org/2005/11/COLLADASchema'}
TNS = '{http://www.collada.org/2005/11/COLLADASchema}'

FILES = [
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae',
    'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae',
]

CHAINS = [
    ('Little', ['Little1', 'Little2', 'Little3']),
    ('Ring', ['Ring1', 'Ring2', 'Ring3']),
    ('Middle', ['Middle1', 'Middle2', 'Middle3']),
    ('Index', ['Index1', 'Index2', 'Index3']),
    ('Thumb', ['Thumb1', 'Thumb2', 'Thumb3']),
]

SLEEVE_X = 9.0


def parse(path):
    tree = ET.parse(path)
    root = tree.getroot()
    src = {}
    for s in root.iter(TNS + 'source'):
        f = s.find('c:float_array', NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        ia = s.find('c:Name_array', NS)
        if ia is not None:
            src[s.get('id')] = ia.text.split()
    mesh = root.find('.//c:geometry/c:mesh', NS)
    tris = mesh.find('c:triangles', NS)
    p_in = tris.find("c:input[@semantic='POSITION']", NS)
    if p_in is None:
        vs = tris.find("c:input[@semantic='VERTEX']", NS).get('source')[1:]
        p_in = mesh.find("c:vertices[@id='%s']/c:input[@semantic='POSITION']" % vs, NS)
    pos = src[p_in.get('source')[1:]].reshape(-1, 3)

    skin = root.find('.//c:controller/c:skin', NS)
    js_id = next(x for x in src if x.endswith('-joints'))
    joints = src[js_id]
    bp_id = next(x for x in src if x.endswith('-bind_poses'))
    bind = src[bp_id].reshape(-1, 16)
    joint_pos = -bind[:, 12:15]  # COLLADA stores matrices column-major

    vw = skin.find('c:vertex_weights', NS)
    vc_el = vw.find('c:vcount', NS)
    v_el = vw.find('c:v', NS)
    vc = np.array([int(x) for x in vc_el.text.split()])
    v = np.array([int(x) for x in v_el.text.split()])
    return tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v


def nearest_on_polyline(p, pts):
    best_d = 1e18
    best_seg = 0
    best_t = 0.0
    for s in range(len(pts) - 1):
        a, b = pts[s], pts[s + 1]
        ab = b - a
        L2 = ab @ ab
        t = ((p - a) @ ab) / L2 if L2 > 0 else 0.0
        tc = min(max(t, 0.0), 1.0)
        q = a + tc * ab
        d = np.linalg.norm(p - q)
        if d < best_d:
            best_d = d
            best_seg = s
            best_t = t
    if best_t > 1.0:
        return best_d, 2  # beyond the last joint -> tip bone
    return best_d, best_seg


def measure_radii(pos, joints, joint_pos):
    idx = {n: i for i, n in enumerate(joints)}
    radii = {}
    for name, bones in CHAINS:
        pts = [joint_pos[idx[b]] for b in bones]
        seg = pts[1] - pts[0]
        seg_len = np.linalg.norm(seg)
        ds = []
        for p in pos:
            if p[0] < SLEEVE_X:
                continue
            t = ((p - pts[0]) @ seg) / (seg_len * seg_len)
            if 0.35 <= t <= 0.7:
                q = pts[0] + t * seg
                d = np.linalg.norm(p - q)
                if d < 3.0:  # only the tube itself, not neighbouring fingers
                    ds.append(d)
        ds = np.array(ds)
        radii[name] = (float(np.percentile(ds, 50)) if len(ds) else 1.0) + 0.45
        print(f'  {name:6s} slice verts {len(ds):4d}  p50={np.percentile(ds,50):.2f} '
              f'p90={np.percentile(ds,90):.2f} -> R = {radii[name]:.2f}')
    return radii


def assign(pos, joints, joint_pos, radii, verbose=False):
    idx = {n: i for i, n in enumerate(joints)}
    chains = [(name, [idx[b] for b in bones]) for name, bones in CHAINS]
    new_bone = np.zeros(len(pos), dtype=int)
    for i, p in enumerate(pos):
        if p[0] < SLEEVE_X:
            new_bone[i] = idx['Hand_Root']
            continue
        best = None
        for name, jidx in chains:
            pts = [joint_pos[j] for j in jidx]
            d, seg = nearest_on_polyline(p, pts)
            if best is None or d < best[0]:
                best = (d, seg, name, jidx)
        d, seg, cname, jidx = best
        if d <= radii[cname]:
            new_bone[i] = jidx[seg] if seg < 3 else jidx[2]
        else:
            new_bone[i] = idx['Palm']
    return new_bone


def report(old_v, old_vc, new_bone, joints, pos):
    nv = len(new_bone)
    counts = np.zeros(len(joints))
    for i in range(nv):
        counts[new_bone[i]] += 1
    total = counts.sum()
    print('  new assignment: ' + ', '.join(
        f'{joints[i]}={int(c)}' for i, c in enumerate(counts) if c > 0))
    moved = sum(1 for i in range(nv) if old_v[2 * i] != new_bone[i])
    print(f'  vertices changed: {moved}/{nv}')
    regs = [
        ('sleeve x<9', lambda p: p[0] < 9),
        ('palm', lambda p: p[0] >= 9),
        ('palm top between knuckles', lambda p: 9 <= p[0] <= 10.6 and p[1] > 3.4 and -4.5 <= p[2] <= 1.5),
        ('palm bottom between tubes', lambda p: 9 <= p[0] <= 13.5 and p[1] < 0.4 and -2 <= p[2] <= 4),
        ('little tube', lambda p: p[0] >= 9.5 and p[2] < -3.5),
        ('ring tube', lambda p: p[0] >= 9.5 and -3.5 <= p[2] < -0.5),
        ('middle tube', lambda p: p[0] >= 9.5 and -0.5 <= p[2] < 4),
        ('index tube', lambda p: p[0] >= 9.5 and 4 <= p[2] < 8),
        ('thumb mass', lambda p: p[0] >= 9.5 and p[2] >= 8),
    ]
    for rname, f in regs:
        sel = [i for i in range(nv) if f(pos[i])]
        if not sel:
            continue
        d = np.zeros(len(joints))
        for i in sel:
            d[new_bone[i]] += 1
        dp = d / len(sel)
        top = np.argsort(dp)[::-1][:3]
        print(f'  {rname:14s}: ' + ', '.join(f'{joints[t]}={dp[t]*100:.0f}%' for t in top))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--dry-run', action='store_true')
    ap.add_argument('--apply', action='store_true')
    args = ap.parse_args()

    for path in FILES:
        print(f'==== {path.split("/")[-1]} ====')
        tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = parse(path)
        print('joints:', list(joints))
        radii = measure_radii(pos, joints, joint_pos)
        new_bone = assign(pos, joints, joint_pos, radii)
        report(v, vc, new_bone, joints, pos)

        if args.apply:
            assert (vc == 1).all(), 'expected single-weight rig'
            pairs = ' '.join(f'{int(b)} 0' for b in new_bone)
            v_el.text = pairs
            vw.set('count', str(len(new_bone)))
            # The HPL1 loader (TinyXML) matches elements by literal name
            # ("asset", "library_geometries", ...). ElementTree would serialize
            # the namespace as a "ns0:" prefix unless we register it as the
            # default namespace, so register it explicitly (matches the
            # original exporter output which used xmlns="...").
            ET.register_namespace('', 'http://www.collada.org/2005/11/COLLADASchema')
            tree.write(path, encoding='UTF-8', xml_declaration=True)
            print(f'  -> written {path}')
        elif args.dry_run:
            print('  (dry-run, nothing written)')


if __name__ == '__main__':
    main()