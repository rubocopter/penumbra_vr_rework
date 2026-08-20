"""Render the hand at bind and at grip=1 (rigid-segment sim, engine pivots),
coloured by dominant bone, three orthographic views. Pure evidence render.

Uses the RUNTIME angles/axes from PlayerHands.cpp (SetupHandAnimation) and
the corrected bind layout (translation at floats 3/7/11): the engine reads
boneWorld = MatrixInverse(INV_BIND_MATRIX), so with the fix each joint's
world position = its mesh position and rotations pivot at the joints.
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
    'Middle1': 70, 'Middle2': 70, 'Middle3': 35,
    'Ring1': 70, 'Ring2': 70, 'Ring3': 35,
    'Little1': 55, 'Little2': 55, 'Little3': 40,
    'Index1': 0, 'Index2': 0, 'Index3': 0,
    'Thumb1': 130, 'Thumb2': 90, 'Thumb3': 0,
}
FINGER_AXIS = np.array([0.0, 0.0, -1.0])     # right hand
PINKY_AXIS = np.array([0.0, -0.8660, -0.5000])  # right hand, curls into the palm
THUMB_AXIS = np.array([0.0, 0.9820, 0.1870])  # right hand, fold plane contains the palm


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
    vc_el = vw.find('c:vcount', fw.NS)
    v_el = vw.find('c:v', fw.NS)
    vc = np.array([int(x) for x in vc_el.text.split()])
    v = np.array([int(x) for x in v_el.text.split()])

    # absolute joint positions from the scene translate chain (corrected layout:
    # -ibm[:,12:15] would read 0 now; the scene is the authoritative meshPos)
    idx = {n: i for i, n in enumerate(joints)}
    joint_pos = np.zeros((len(joints), 3))
    local_trans = np.zeros((len(joints), 3))
    vs = root.find('.//c:visual_scene', fw.NS)
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
    walk(vs, None, np.zeros(3))
    return tree, root, pos, joints, joint_pos, local_trans, vw, vc_el, v_el, vc, v, ibm


def main():
    tree, root, pos, joints, joint_pos, local_trans, vw, vc_el, v_el, vc, v, ibm = parse(PATH)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)

    # engine skinning (fixed layout): stored IBM = (I, -meshPos), animated bone
    # world = (R_accum, meshPos); vertex' = meshPos + R_accum . (v - meshPos)
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

    def pose(deg_by_joint, local_trans):
        ws = []
        for i in range(len(joints)):
            name = joints[i]
            deg = deg_by_joint.get(name, 0.0)
            R = rmat(PINKY_AXIS if name.startswith('Little') else
                     THUMB_AXIS if name.startswith('Thumb') else FINGER_AXIS, deg) if deg else np.eye(3)
            L = np.eye(4)
            L[:3, :3] = R
            L[:3, 3] = local_trans[i]
            ws.append(L if parent[i] is None else ws[parent[i]] @ L)
        return ws

    ws_grip = pose(GRIP, local_trans)

    # weights: single (joint, weightIdx) pair per vertex, weight value 1.0
    pos_out = np.zeros_like(pos)
    for i, p in enumerate(pos):
        jidx = int(v[2 * i])
        pos_out[i] = (ws_grip[jidx] @ np.append(p - joint_pos[jidx], 1.0))[:3]

    # render
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    tris = []
    tri = root.find('.//c:mesh/c:triangles', fw.NS)
    ins = tri.findall('c:input', fw.NS)
    stride = max(int(i.get('offset')) for i in ins) + 1
    p = [int(x) for x in tri.find('c:p', fw.NS).text.split()]
    for i in range(0, len(p) - 2 * stride, stride):
        tris.append([p[i], p[i + stride], p[i + 2 * stride]])
    colors = plt.cm.tab20(np.array([new_bone[i] % 20 for i in range(len(pos))]) / 20.0)
    # group tris by average bone for per-bone shading
    tris_by_bone = {}
    for t in tris:
        b = int(np.round(np.mean([new_bone[i] for i in t])))
        tris_by_bone.setdefault(b, []).append(t)

    views = [('front (-X): palm up', lambda p: (p[2], -p[1])),
             ('side (-Z): fingers -> right', lambda p: (p[0], -p[1])),
             ('top (+Y): palm down', lambda p: (p[0], -p[2]))]
    names = ['bind', 'grip']
    for vi, (vname, proj) in enumerate(views):
        fig, axs = plt.subplots(1, 2, figsize=(13, 6.5))
        for ai, (nm, P) in enumerate([('bind', pos), ('grip=1', pos_out)]):
            ax = axs[ai]
            ax.set_title('%s | %s' % (vname, nm))
            pc = []
            for b, tt in sorted(tris_by_bone.items()):
                pts = np.array([proj(P[i]) for i in tt[0]])
                if len(tt) > 1:
                    pts = np.array([proj(P[i]) for t in tt for i in t])
                else:
                    pts = np.array([proj(P[i]) for i in tt[0]])
                pc.append(pts)
            col = PolyCollection(pc, facecolors=colors[[b for b in sorted(tris_by_bone)]],
                                 edgecolors='none')
            ax.add_collection(col)
            ax.autoscale()
            ax.set_aspect('equal')
            ax.set_xlabel('x' if 'side' in vname or 'top' in vname else 'z')
        fig.tight_layout()
        fig.savefig(r'E:\penumbra_vr-master\scripts\render_%d.png' % vi, dpi=110)
        plt.close(fig)
    print('rendered 3 views x 2 poses')


if __name__ == '__main__':
    main()