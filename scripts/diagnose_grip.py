"""Numeric deformation report at grip=1: edge stretch (seam tears) and fold
angles, per finger. Evidence for the diagnosis (hypothesis E).
"""

import sys
import xml.etree.ElementTree as ET

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

PATH = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
RAD = np.pi / 180.0

GRIP = {n: {'x': a[0], 'y': a[1], 'z': a[2]} for n, a in {
    'Little1': (60, 0, 0), 'Little2': (60, 0, 0), 'Little3': (45, 0, 0),
    'Ring1': (60, 0, 0), 'Ring2': (60, 0, 0), 'Ring3': (45, 0, 0),
    'Middle1': (60, 0, 0), 'Middle2': (60, 0, 0), 'Middle3': (45, 0, 0),
    'Index1': (60, 0, 0), 'Index2': (60, 0, 0), 'Index3': (45, 0, 0),
    'Thumb1': (45, 0, 0), 'Thumb2': (45, 0, 0), 'Thumb3': (40, 0, 0),
    'Palm': (0, 0, 0), 'Hand_Root': (0, 0, 0),
}.items()}


def rmat(axis, deg):
    a = np.zeros(3); a[{'x': 0, 'y': 1, 'z': 2}[axis]] = 1.0
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def main():
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(PATH)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)

    skin = root.find('.//c:controller/c:skin', fw.NS)
    ibm_id = skin.find("c:joints/c:input[@semantic='INV_BIND_MATRIX']", fw.NS).get('source')[1:]
    src = {}
    for s in root.iter(fw.TNS + 'source'):
        f = s.find('c:float_array', fw.NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
    ibm = src[ibm_id].reshape(-1, 16)
    bind_world = np.array([np.linalg.inv(ibm[i].reshape(4, 4)) for i in range(len(joints))])

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

    locs = [bind_world[i] if parent[i] is None else
            np.linalg.inv(bind_world[parent[i]]) @ bind_world[i]
            for i in range(len(joints))]

    def pose(deg_by_joint):
        ws = []
        for i in range(len(joints)):
            R = np.eye(3)
            for ax in ('x', 'y', 'z'):
                R = R @ rmat(ax, deg_by_joint.get(joints[i], {}).get(ax, 0.0))
            T = np.eye(4); T[:3, :3] = R; T[3, :3] = locs[i][3, :3]
            world = T if parent[i] is None else ws[parent[i]] @ T
            ws.append(world)
        return ws

    zero = {j: {'x': 0, 'y': 0, 'z': 0} for j in joints}
    bind_ws = pose(zero)
    grip_ws = pose(GRIP)
    skinm = np.array([grip_ws[i] @ np.linalg.inv(bind_ws[i]) for i in range(len(joints))])

    wsrc = src[skin.find("c:vertex_weights/c:input[@semantic='WEIGHT']", fw.NS).get('source')[1:]]
    pos_out = np.zeros_like(pos)
    vp = 0
    for i, p in enumerate(pos):
        h = np.append(p, 1.0)
        acc = np.zeros(4)
        for _ in range(int(vc[i])):
            acc += float(wsrc[int(v[2 * vp + 1])]) * (skinm[int(v[2 * vp])] @ h)
            vp += 1
        pos_out[i] = acc[:3]

    # ---- edge stretch: mesh continuity break ----
    mesh = root.find('.//c:geometry/c:mesh', fw.NS)
    tris_el = mesh.find('c:triangles', fw.NS)
    n_in = len(list(tris_el.findall('c:input', fw.NS)))
    pidx = np.array([int(x) for x in tris_el.find('c:p', fw.NS).text.split()])
    edges = {}
    for i in range(0, len(pidx), 3 * n_in):
        tri = [pidx[i + k * n_in] for k in range(3)]
        for k in range(3):
            a, b = tri[k], tri[(k + 1) % 3]
            edges.setdefault(tuple(sorted((a, b))), True)
    stretch = []
    for (a, b) in edges:
        l0 = np.linalg.norm(pos[a] - pos[b])
        l1 = np.linalg.norm(pos_out[a] - pos_out[b])
        if l0 > 0:
            stretch.append((l1 / l0, a, b))
    stretch.sort(reverse=True)
    print('-- edge stretch at grip=1 (ratio >1 = torn open; <1 = compressed) --')
    for r, a, b in stretch[:12]:
        print('ratio %.3f  verts %d(%s x%.1f) %d(%s x%.1f)  at (%.2f,%.2f,%.2f)' % (
            r, a, joints[new_bone[a]], pos[a][0], b, joints[new_bone[b]], pos[b][0],
            *(pos[a] + pos[b]) / 2))

    # ---- fold angles at grip=1: direction of each segment vs its parent ----
    print('\n-- segment directions at grip=1 (crease angles) --')
    for cname, bones in fw.CHAINS:
        jidx = [idx[b] for b in bones]
        for k in range(2):
            J0 = grip_ws[jidx[k]][3, :3]
            J1 = grip_ws[jidx[k + 1]][3, :3]
            d = J1 - J0
            d /= np.linalg.norm(d)
            if k == 0:
                ang = np.degrees(np.arccos(np.clip(d @ np.array([1, 0, 0]), -1, 1)))
            else:
                d0 = grip_ws[jidx[k]][3, :3] - grip_ws[jidx[k - 1]][3, :3]
                d0 /= np.linalg.norm(d0)
                ang = np.degrees(np.arccos(np.clip(d @ d0, -1, 1)))
            print('%s: segment %d dir=(%.2f,%.2f,%.2f) bend=%.0f deg' % (cname, k + 1, *d, ang))

    # ---- visible finger composition ----
    print('\n-- visible finger vs bones (webbing line x where tubes separate) --')
    for cname, bones in fw.CHAINS:
        jidx = [idx[b] for b in bones]
        print('%s: X1 region x %.1f..%.1f | X2 x %.1f..%.1f | X3 x %.1f..%.1f' % (
            cname,
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[0]]][:, 0].min(),
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[0]]][:, 0].max(),
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[1]]][:, 0].min(),
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[1]]][:, 0].max(),
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[2]]][:, 0].min(),
            pos[[i for i in range(len(pos)) if new_bone[i] == jidx[2]]][:, 0].max()))

    # ---- palm/tube boundary separation ----
    print('\n-- palm-body vs finger seam at grip=1 --')
    palmv = [i for i in range(len(pos)) if new_bone[i] == idx['Palm']]
    palm_span = (pos[palmv][:, 0].min(), pos[palmv][:, 0].max())
    print('palm body region x %.1f..%.1f (%d verts) rotates around pivot x=%.2f' % (
        palm_span[0], palm_span[1], len(palmv), grip_ws[idx['Palm']][3, 0]))
    print('finger tubes X1 pivot at x=%.2f (Little) .. %.2f (Ring) — INSIDE the palm body' % (
        grip_ws[idx['Little1']][3, 0], grip_ws[idx['Ring1']][3, 0]))


if __name__ == '__main__':
    main()