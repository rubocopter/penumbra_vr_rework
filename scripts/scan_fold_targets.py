import sys, os, math
import numpy as np
NS = 'http://www.collada.org/2005/11/COLLADASchema'
TNS = '{%s}' % NS

RAD = math.pi / 180.0
RIGHT = r'E:\penumbra_vr-master\data\models\hud_objects\hud_object_hand_rig.dae'
LEFT = r'E:\penumbra_vr-master\data\models\hud_objects\hud_object_hand_left_rig.dae'


def parse(path):
    import xml.etree.ElementTree as ET
    tree = ET.parse(path)
    root = tree.getroot()
    src = {}
    NSM = {'c': NS}
    for s in root.iter(TNS + 'source'):
        f = s.find('c:float_array', NSM)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        na = s.find('c:Name_array', NSM)
        if na is not None:
            src[s.get('id')] = na.text.split()
    mesh = root.find('.//c:geometry/c:mesh', NSM)
    tris = mesh.find('c:triangles', NSM)
    p_in = tris.find("c:input[@semantic='POSITION']", NSM)
    if p_in is None:
        vs = tris.find("c:input[@semantic='VERTEX']", NSM).get('source')[1:]
        p_in = mesh.find("c:vertices[@id='%s']/c:input[@semantic='POSITION']" % vs, NSM)
    pos = src[p_in.get('source')[1:]].reshape(-1, 3)
    skin = root.find('.//c:controller/c:skin', NSM)
    js_id = next(x for x in src if x.endswith('-joints'))
    joints = src[js_id]
    bp_id = next(x for x in src if x.endswith('-bind_poses'))
    ibm = src[bp_id].reshape(-1, 16)
    vw = skin.find('c:vertex_weights', NSM)
    vc = np.array([int(x) for x in vw.find('c:vcount', NSM).text.split()])
    v = np.array([int(x) for x in vw.find('c:v', NSM).text.split()])
    idx = {n: i for i, n in enumerate(joints)}
    joint_pos = np.zeros((len(joints), 3))
    local_trans = np.zeros((len(joints), 3))
    parent = {idx['Hand_Root']: None}
    vs_el = root.find('.//c:visual_scene', NSM)
    def walk(node, p, acc):
        name = node.get('name')
        t = node.find('c:translate', {'c': NS})
        local = np.array([float(x) for x in t.text.split()]) if t is not None else np.zeros(3)
        acc2 = local if p is None else acc + local
        if name in idx:
            joint_pos[idx[name]] = acc2
            local_trans[idx[name]] = local
            parent[idx[name]] = p
            p = idx[name]
        for ch in list(node):
            if ch.tag == TNS + 'node':
                walk(ch, p, acc2)
    walk(vs_el, None, np.zeros(3))
    return joints, pos, joint_pos, local_trans, parent, vc, v, idx


def rmat(axis, deg):
    a = np.asarray(axis, dtype=float)
    a = a / np.linalg.norm(a)
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def fold(joints, pos, joint_pos, local_trans, parent, vc, v, idx, deg_by_joint, axis_for, name3):
    n = len(joints)
    ws = []
    for i in range(n):
        nm = joints[i]
        deg = deg_by_joint.get(nm, 0.0)
        R = rmat(axis_for(nm), deg) if deg else np.eye(3)
        L = np.eye(4)
        L[:3, :3] = R
        L[:3, 3] = local_trans[i]
        ws.append(L if parent[i] is None else ws[parent[i]] @ L)
    out = np.zeros_like(pos)
    for i, p in enumerate(pos):
        jidx = int(v[2 * i])
        out[i] = (ws[jidx] @ np.append(p - joint_pos[jidx], 1.0))[:3]
    sel = [i for i in range(len(vc)) if int(v[2 * i]) == idx[name3]]
    tip = out[sel]
    cen_b = pos[sel].mean(axis=0)
    cen_f = tip.mean(axis=0)
    return cen_b, cen_f, np.linalg.norm(cen_f - cen_b), cen_f - cen_b


def scan_pinky(joints, pos, joint_pos, local_trans, parent, vc, v, idx, left):
    base, palm = (np.array([9.16, 2.23, -5.98]), np.array([9.21, 1.69, 2.88])) if not left else \
                 (np.array([9.16, -2.23, -5.98]), np.array([9.21, -1.69, 2.88]))
    fdir = 1.0 if left else -1.0
    print('pinky base=%s palm_centre=%s' % (np.round(base, 2), np.round(palm, 2)))
    print(' tilt  end_pos           |move|  dir        |dist to palm')
    best = None
    for tilt_deg in range(-40, 61, 5):
        ax = np.array([0.0, -math.sin(tilt_deg * RAD), fdir * math.cos(tilt_deg * RAD)])
        degs = {'Little1': 55, 'Little2': 55, 'Little3': 40}
        cb, cf, mv, d = fold(joints, pos, joint_pos, local_trans, parent, vc, v, idx, degs,
                             lambda nm, ax=ax: ax, 'Little3')
        dist = np.linalg.norm(cf - palm)
        mark = ''
        if best is None or dist < best[0]:
            best = (dist, tilt_deg, cf)
        if dist < 4.0:
            mark = ' <--'
        print(' %+4d  %s  %5.2f  %s  %5.2f%s' % (tilt_deg, np.round(cf, 2), mv,
                                                 np.round(d / np.linalg.norm(d), 2), dist, mark))
    print('BEST: tilt=%+d end=%s dist=%.2f' % (best[1], np.round(best[2], 2), best[0]))


def scan_thumb(joints, pos, joint_pos, local_trans, parent, vc, v, idx, left):
    palm = np.array([9.21, 1.69, 2.88]) if not left else np.array([9.21, -1.69, 2.88])
    print('thumb: candidate axes (right), angles -> tip3 end')
    cands = [(0.8910, 0.4540, 'current'), (0.9400, 0.3400, 'mid'),
             (0.9820, 0.1870, 'ideal'), (0.9960, 0.0870, 'near-y'),
             (0.9700, 0.2420, 'ideal2')]
    if left:
        cands = [(a1, -a2, n) for (a1, a2, n) in cands]
    for (a1, a2, nm) in cands:
        for (g1, g2) in [(95, 80), (110, 80), (125, 85)]:
            degs = {'Thumb1': g1, 'Thumb2': g2}
            cb, cf, mv, d = fold(joints, pos, joint_pos, local_trans, parent, vc, v, idx, degs,
                                 lambda nm_, a1=a1, a2=a2: np.array([0.0, a1, a2]), 'Thumb3')
            print('  %-7s %3d/%3d (cum %3d): tip_end=%s |move|=%.2f dist_to_palm=%.2f'
                  % (nm, g1, g2, g1 + g2, np.round(cf, 2), mv, np.linalg.norm(cf - palm)))
    print()


for path, left in [(RIGHT, False), (LEFT, True)]:
    joints, pos, joint_pos, local_trans, parent, vc, v, idx = parse(path)
    print('=' * 90)
    print('HAND: %s' % ('LEFT' if left else 'RIGHT'))
    scan_pinky(joints, pos, joint_pos, local_trans, parent, vc, v, idx, left)
    scan_thumb(joints, pos, joint_pos, local_trans, parent, vc, v, idx, left)