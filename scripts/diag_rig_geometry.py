"""Diagnose why the thumb does not visibly fold although the engine applies the
full rotation, and why the pinky still folds toward the middle finger.

Prints, for every bone: bind joint position, segment direction, angle between
the segment and its assigned animation axis, and (grip sim) the joint chain
tip displacement. Also prints vertex-displacement evidence for the thumb mesh.

Usage: python scripts/diag_rig_geometry.py
"""

import sys
import xml.etree.ElementTree as ET

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

PATH = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
RAD = np.pi / 180.0

# runtime grab angles from PlayerHands.cpp (grip weight = 1)
GRIP = {
    'Middle1': 17, 'Middle2': 17, 'Middle3': 10,
    'Ring1': 17, 'Ring2': 17, 'Ring3': 10,
    'Little1': 30, 'Little2': 30, 'Little3': 20,
    'Index1': 0, 'Index2': 0, 'Index3': 0,
    'Thumb1': 130, 'Thumb2': 90, 'Thumb3': 0,
}
FINGER_AXIS = np.array([0.0, 0.0, -1.0])        # right hand
PINKY_AXIS = np.array([0.0, -0.8660, -0.5000])  # right hand, curls into the palm
THUMB_AXIS = np.array([0.065, 0.9970, -0.0460])  # right hand, plane through chain and palm centre


def rmat(axis, deg):
    a = np.asarray(axis, dtype=float)
    a = a / np.linalg.norm(a)
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def parse(path):
    tree = ET.parse(path)
    root = tree.getroot()
    src = {}
    for s in root.iter(fw.TNS + 'source'):
        f = s.find('c:float_array', fw.NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        na = s.find('c:Name_array', fw.NS)
        if na is not None:
            src[s.get('id')] = na.text.split()
    mesh = root.find('.//c:geometry/c:mesh', fw.NS)
    tris = mesh.find('c:triangles', fw.NS)
    p_in = tris.find("c:input[@semantic='POSITION']", fw.NS)
    if p_in is None:
        vs = tris.find("c:input[@semantic='VERTEX']", fw.NS).get('source')[1:]
        p_in = mesh.find("c:vertices[@id='%s']/c:input[@semantic='POSITION']" % vs, fw.NS)
    pos = src[p_in.get('source')[1:]].reshape(-1, 3)

    skin = root.find('.//c:controller/c:skin', fw.NS)
    js_id = next(x for x in src if x.endswith('-joints'))
    joints = src[js_id]
    bp_id = next(x for x in src if x.endswith('-bind_poses'))
    ibm = src[bp_id].reshape(-1, 16)

    vw = skin.find('c:vertex_weights', fw.NS)
    vc = np.array([int(x) for x in vw.find('c:vcount', fw.NS).text.split()])
    v = np.array([int(x) for x in vw.find('c:v', fw.NS).text.split()])

    idx = {n: i for i, n in enumerate(joints)}
    joint_pos = np.zeros((len(joints), 3))
    local_trans = np.zeros((len(joints), 3))
    vs_el = root.find('.//c:visual_scene', fw.NS)
    def walk(node, p, acc):
        name = node.get('name')
        t = node.find('c:translate', fw.NS)
        local = np.array([float(x) for x in t.text.split()]) if t is not None else np.zeros(3)
        acc2 = local if p is None else acc + local
        if name in idx:
            joint_pos[idx[name]] = acc2
            local_trans[idx[name]] = local
            p = idx[name]
        for ch in list(node):
            if ch.tag == fw.TNS + 'node':
                walk(ch, p, acc2)
    walk(vs_el, None, np.zeros(3))
    return tree, root, pos, joints, joint_pos, local_trans, vc, v, ibm


def axis_for(name):
    if name.startswith('Little'):
        return PINKY_AXIS
    if name.startswith('Thumb'):
        return THUMB_AXIS
    return FINGER_AXIS


def main():
    tree, root, pos, joints, joint_pos, local_trans, vc, v, ibm = parse(PATH)
    idx = {n: i for i, n in enumerate(joints)}
    n = len(joints)

    parent = {idx['Hand_Root']: None}
    vs = root.find('.//c:visual_scene', fw.NS)
    def walk(node, p):
        name = node.get('name')
        if name in idx:
            parent[idx[name]] = p
        for ch in list(node):
            if ch.tag == fw.TNS + 'node':
                walk(ch, idx.get(name))
    walk(vs, None)

    bind_world = np.array([np.linalg.inv(ibm[i].reshape(4, 4)) for i in range(n)])
    def local_from_world(w, p):
        if p is None:
            return w
        return np.linalg.inv(p) @ w

    locs = [local_from_world(bind_world[i], bind_world[parent[i]] if parent[i] is not None else None)
            for i in range(n)]
    bind_pos = locs

    # engine skinning (verified model, Node3D::UpdateMatrix + skin):
    # local = (R_anim, local_trans); world = parent * local (accumulated);
    # vertex' = world_j . (v - joint_pos_j)
    def pose(deg_by_joint):
        ws = []
        for i in range(n):
            name = joints[i]
            deg = deg_by_joint.get(name, 0.0)
            R = rmat(axis_for(name), deg) if deg else np.eye(3)
            L = np.eye(4)
            L[:3, :3] = R
            L[:3, 3] = local_trans[i]
            ws.append(L if parent[i] is None else ws[parent[i]] @ L)
        return ws

    def skinned(ws, verts):
        out = np.zeros_like(verts)
        for i, p in enumerate(verts):
            jidx = int(v[2 * i])
            out[i] = (ws[jidx] @ np.append(p - joint_pos[jidx], 1.0))[:3]
        return out

    print('=== segment directions vs assigned axis (right hand) ===')
    chains = {}
    for i in range(n):
        name = joints[i]
        ch = name.rstrip('0123')
        chains.setdefault(ch, []).append(i)
    def chain_order(k):
        return sorted(chains[k], key=lambda i: (int(''.join(c for c in joints[i] if c.isdigit()) or 0)))
    for k in sorted(chains):
        js = chain_order(k)
        names = [joints[i] for i in js]
        pts = [joint_pos[i] for i in js]
        print('%s: joints %s' % (k, names))
        for a, b in zip(js[:-1], js[1:]):
            seg = joint_pos[b] - joint_pos[a]
            L = np.linalg.norm(seg)
            if L > 1e-9:
                seg = seg / L
            ax = axis_for(joints[a])
            ax = ax / np.linalg.norm(ax)
            ang = np.degrees(np.arccos(np.clip(np.dot(seg, ax), -1, 1)))
            print('  %s -> %s  seg=%.3f  axis=%.3f  angle(seg,axis)=%.1f deg'
                  % (joints[a], joints[b], L, ang if L > 1e-9 else -1, ang if L > 1e-9 else -1))
        print('  base=%s tip_joint=%s  chain_len=%.3f'
              % (np.round(joint_pos[js[0]], 3), np.round(joint_pos[js[-1]], 3),
                 np.linalg.norm(joint_pos[js[-1]] - joint_pos[js[0]])))

    # palm centre: Middle1 base joint
    palm = joint_pos[idx['Middle1']]
    print('\n=== palm centre (Middle1 base = %s) ===' % np.round(palm, 3))

    # thumb chain detail: fold plane
    t = chain_order('Thumb')
    s1 = joint_pos[t[1]] - joint_pos[t[0]]
    s2 = joint_pos[t[2]] - joint_pos[t[1]]
    s1u, s2u = s1 / np.linalg.norm(s1), s2 / np.linalg.norm(s2)
    plane_n = np.cross(s1u, s2u)
    print('\n=== thumb chain ===')
    print('  seg1=%s seg2=%s' % (np.round(s1u, 3), np.round(s2u, 3)))
    print('  chain plane normal (cross s1,s2) = %s' % np.round(plane_n / np.linalg.norm(plane_n), 3))
    print('  thumb axis = %s  |dot(axis,plane_n)| = %.3f'
          % (THUMB_AXIS, abs(np.dot(THUMB_AXIS / np.linalg.norm(THUMB_AXIS), plane_n / np.linalg.norm(plane_n)))))
    print('  thumb axis vs seg1: %.1f deg, vs seg2: %.1f deg'
          % (np.degrees(np.arccos(np.clip(np.dot(THUMB_AXIS / np.linalg.norm(THUMB_AXIS), s1u), -1, 1))),
             np.degrees(np.arccos(np.clip(np.dot(THUMB_AXIS / np.linalg.norm(THUMB_AXIS), s2u), -1, 1)))))

    # verify layout assumption: stored IBM = (I, -meshPos) -> boneWorld = (I, meshPos)
    print('=== layout check ===')
    for bn in ['Hand_Root', 'Palm', 'Middle1', 'Thumb1', 'Thumb3']:
        bi = idx[bn]
        raw = ibm[bi].reshape(4, 4)
        w = np.linalg.inv(raw)
        print('  %s: ibm_rot_max=%.2e ibm_trans=%s inv_trans=%s scene=%s'
              % (bn, np.max(np.abs(raw[:3, :3] - np.eye(3))), np.round(raw[:3, 3], 4),
                 np.round(w[:3, 3], 4), np.round(joint_pos[bi], 4)))

    # vertex displacement evidence (engine-exact skinning)
    ws_grip = pose(GRIP)
    pos_out = skinned(ws_grip, pos)

    def verts_of(bones):
        bs = {idx[b] for b in bones}
        return [i for i in range(len(vc)) if int(v[2 * i]) in bs]

    print('\n=== vertex displacement per chain (grip=1) ===')
    for k in ['Middle', 'Ring', 'Little', 'Index', 'Thumb']:
        js = chain_order(k)
        sel = verts_of([joints[i] for i in js])
        if not sel:
            print('  %s: NO vertices weighted to its bones' % k)
            continue
        d = np.linalg.norm(pos_out[sel] - pos[sel], axis=1)
        dirm = np.mean(pos_out[sel] - pos[sel], axis=0)
        dirm = dirm / (np.linalg.norm(dirm) + 1e-12)
        print('  %s: %d verts, mean|disp| = %.3f, max = %.3f, mean dir = %s'
              % (k, len(sel), d.mean(), d.max(), np.round(dirm, 3)))
        bi = idx[joints[js[-1]]]
        tip_sel = [i for i in range(len(vc)) if int(v[2 * i]) == bi]
        if tip_sel:
            d2 = np.linalg.norm(pos_out[tip_sel] - pos[tip_sel], axis=1)
            print('    %s (tip bone): %d verts, mean|disp| = %.3f' % (joints[js[-1]], len(tip_sel), d2.mean()))

    print('\n=== per-bone vertex counts + displacement ===')
    for i in range(n):
        sel = [j for j in range(len(vc)) if int(v[2 * j]) == i]
        if not sel:
            print('  %s: NO vertices' % joints[i])
            continue
        d = np.linalg.norm(pos_out[sel] - pos[sel], axis=1)
        print('  %s: %4d verts  mean|disp|=%.3f  max=%.3f'
              % (joints[i], len(sel), d.mean(), d.max()))

    print('\n=== grip pose geometry (vertices) ===')
    for k in ['Middle', 'Ring', 'Little', 'Thumb']:
        js = chain_order(k)
        sel = verts_of([joints[i] for i in js])
        if not sel:
            continue
        b = np.array([pos[i] for i in sel])
        g = np.array([pos_out[i] for i in sel])
        c0, c1 = b.mean(axis=0), g.mean(axis=0)
        print('  %s: centroid bind=%s grip=%s  |move|=%.3f' % (k, np.round(c0, 3), np.round(c1, 3),
                                                               np.linalg.norm(c1 - c0)))


if __name__ == '__main__':
    main()
