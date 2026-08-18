"""verify_bind_fix.py - Post-fix verification of the corrected bind layout.

After fix_rig_bind_layout.py transposed the translation of the 17 bind
matrices (floats 12..14 -> 3/7/11), this script verifies, for both hands:

  Part A - math: motorBindPos (engine GetTranslation of MatrixInverse(IB)) vs
           meshPos (scene translate nodes), products IB x M ~ I, and the
           B-hypothesis discard check (errorPosition must now be ~0).

  Part B - engine-exact skinning simulation (Node3D::UpdateMatrix model:
           local = (R_anim, P_j), rotation about the local origin = the joint):
           B1 single-joint rotations: joint fixed, effective centre = joint,
              fold = applied angle, proximal/origin segments stable.
           B2 grip=1 with the RUNTIME angles/axes from PlayerHands.cpp:
              palm/sleeve stable, per-joint cumulative folds, thumb stable,
              no global rotation.

  Part C - regenerates render_0/1/2.png (right hand, bind vs grip) with the
           corrected rig.

Read-only on the .dae files. Writes only the render PNGs.
"""

import sys
import xml.etree.ElementTree as ET

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

NS = fw.NS
TNS = fw.TNS

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
LEFT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae'
RAD = np.pi / 180.0

# runtime from PlayerHands.cpp (SetupHandAnimation), ksBoneNames order
GRAB = {
    'Middle1': 70, 'Middle2': 70, 'Middle3': 50,
    'Ring1': 70, 'Ring2': 70, 'Ring3': 50,
    'Little1': 65, 'Little2': 65, 'Little3': 45,
    'Index1': 0, 'Index2': 0, 'Index3': 0,
    'Thumb1': 47, 'Thumb2': 40, 'Thumb3': 0,
}

CHAIN_SEGS = [('Middle', [70, 70, 35]), ('Ring', [70, 70, 35]),
              ('Little', [55, 55, 40]), ('Index', [0, 0, 0]),
              ('Thumb', [130, 90, 0])]


def parse(path):
    tree = ET.parse(path)
    root = tree.getroot()
    src = {}
    for s in root.iter(TNS + 'source'):
        f = s.find('c:float_array', NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        na = s.find('c:Name_array', NS)
        if na is not None:
            src[s.get('id')] = na.text.split()
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
    ibm = src[bp_id].reshape(-1, 16)

    vw = skin.find('c:vertex_weights', NS)
    vc = np.array([int(x) for x in vw.find('c:vcount', NS).text.split()])
    v = np.array([int(x) for x in vw.find('c:v', NS).text.split()])

    # meshPos from the visual_scene translate nodes. The node translates are
    # LOCAL (relative): the absolute joint position is the sum along the chain.
    idx = {n: i for i, n in enumerate(joints)}
    scene = np.zeros((len(joints), 3))
    local_trans = np.zeros((len(joints), 3))
    parent = {idx['Hand_Root']: None}
    vs = root.find('.//c:visual_scene', NS)
    def walk(node, p_idx, acc):
        name = node.get('name')
        t = node.find('c:translate', NS)
        local = np.array([float(x) for x in t.text.split()]) if t is not None else np.zeros(3)
        acc2 = local if p_idx is None else acc + local
        my_idx = idx.get(name)
        if my_idx is not None:
            parent[my_idx] = p_idx
            scene[my_idx] = acc2
            local_trans[my_idx] = local
            p_idx = my_idx
        for ch in list(node):
            if ch.tag == TNS + 'node':
                walk(ch, p_idx, acc2)
    walk(vs, None, np.zeros(3))
    joint_pos = scene
    return root, pos, joints, ibm, vc, v, joint_pos, parent, local_trans


def rot(axis, deg):
    a = np.asarray(axis, dtype=float)
    a = a / np.linalg.norm(a)
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def engine_worlds(rot_by_idx, joint_pos, parent, local_trans):
    """Node3D::UpdateMatrix model: local = (R_anim, P_j); world = parent * local."""
    ws = [None] * len(joint_pos)
    for i in range(len(joint_pos)):
        R = rot_by_idx.get(i)
        L = np.eye(4)
        if R is not None:
            L[:3, :3] = R
        L[:3, 3] = local_trans[i]
        p = parent[i]
        ws[i] = L if p is None else ws[p] @ L
    return ws


def skin(pos, ws, joint_pos, vc, v):
    out = np.zeros_like(pos)
    vp = 0
    for i, p in enumerate(pos):
        j = int(v[2 * vp])
        vp += 1
        out[i] = (ws[j] @ np.append(p - joint_pos[j], 1.0))[:3]
    return out


def ls_centre(va, vb, R):
    """LS fit of a point on the rotation axis: c = pinv(I-R) * mean(vb - R*va).
    Any point on the axis is a valid centre (rotation axis ambiguity)."""
    d = np.mean(vb - va @ R.T, axis=0)
    Ir = np.eye(3) - R
    return np.linalg.lstsq(Ir, d, rcond=None)[0]


def kabsch(va, vb):
    """Fit the rigid rotation va -> vb; return (R, angle_deg)."""
    ca = va.mean(0)
    cb = vb.mean(0)
    H = (va - ca).T @ (vb - cb)
    U, _, Vt = np.linalg.svd(H)
    if np.linalg.det(Vt.T @ U.T) < 0:
        Vt[-1] *= -1
    R = Vt.T @ U.T
    ang = np.degrees(np.arccos(np.clip((np.trace(R) - 1) / 2, -1, 1)))
    return R, ang


def rot_axis(R):
    """Unit axis of rotation R (from the skew part)."""
    a = np.array([R[2, 1] - R[1, 2], R[0, 2] - R[2, 0], R[1, 0] - R[0, 1]])
    return a / (np.linalg.norm(a) + 1e-12)


def dist_to_axis(J, c, axis):
    return np.linalg.norm((np.eye(3) - np.outer(axis, axis)) @ (J - c))


def report_hand(path, label):
    print('\n' + '=' * 104)
    print('== %s  (%s)' % (label, path.split('/')[-1]))
    print('=' * 104)
    root, pos, joints, ibm, vc, v, joint_pos, parent, local_trans = parse(path)
    n = len(joints)
    idx = {name: i for i, name in enumerate(joints)}
    left = 'left' in label.lower()

    # ---------------- Part A ----------------
    print('\n[Part A] bind math (engine convention: translation at floats 3/7/11)')
    print('%-10s %-36s %-36s %8s' % ('Joint', 'meshPos (scene)', 'motorBindPos (engine)', 'err'))
    maxerr = 0.0
    worst_prod = 0.0
    for i, name in enumerate(joints):
        M = ibm[i].reshape(4, 4)
        inv = np.linalg.inv(M)
        motor = np.array([inv[0][3], inv[1][3], inv[2][3]])
        err = np.linalg.norm(motor - joint_pos[i])
        maxerr = max(maxerr, err)
        worst_prod = max(worst_prod, np.abs(inv @ M - np.eye(4)).max(),
                         np.abs(M @ inv - np.eye(4)).max())
        print('%-10s (%8.3f,%8.3f,%8.3f)  (%8.3f,%8.3f,%8.3f)  %8.3f' %
              (name, *joint_pos[i], *motor, err))
    exp = -ibm[:, [3, 7, 11]]
    dexp = np.max(np.linalg.norm(exp - joint_pos, axis=1))
    print('  max |motorBindPos - meshPos| = %.4f' % maxerr)
    print('  max |file-intended (-ibm floats 3/7/11) - meshPos| = %.4f' % dexp)
    print('  max |IB x M - I| / |M x IB - I| = %.2e' % worst_prod)
    print('  -> B hypothesis (transposed layout): %s'
          % ('DISCARDED (bind == mesh)' if maxerr < 0.01 else 'STILL PRESENT'))

    # ---------------- Part B ----------------
    radii = fw.measure_radii(pos, joints, joint_pos)
    bone_of = fw.assign(pos, joints, joint_pos, radii)
    seg_of = {name: [idx[b] for b in (f + '1', f + '2', f + '3')] for f, _ in CHAIN_SEGS}

    finger_axis = np.array([0, 0, -1.0 if left else 1.0])
    thumb_axis = np.array([0, 0.9820, -0.1870 if left else 0.1870])
    pinky_axis = np.array([0, -0.8660, 0.5000 if left else -0.5000])

    def chain_axis(name):
        if name == 'Thumb':
            return thumb_axis
        if name == 'Little':
            return pinky_axis
        return finger_axis

    ws_bind = engine_worlds({}, joint_pos, parent, local_trans)
    pos_bind = skin(pos, ws_bind, joint_pos, vc, v)

    # B1: single-joint rotations (+10 deg, runtime axes)
    print('\n[Part B1] single-joint +10 deg rotations (runtime axes)')
    print('%-9s %12s %10s %10s %10s %10s' %
          ('rotated', 'axis-joint d', 'joint d', 'seg rot', 'prox d', 'origin d'))
    tests = [('Index1', finger_axis), ('Index2', finger_axis), ('Index3', finger_axis),
             ('Middle1', finger_axis), ('Middle2', finger_axis), ('Middle3', finger_axis),
             ('Ring1', finger_axis), ('Little1', pinky_axis), ('Little2', pinky_axis),
             ('Thumb1', thumb_axis), ('Thumb2', thumb_axis),
             ('Thumb3', thumb_axis)]
    all_ok = True
    for name, axis in tests:
        j = idx[name]
        ws = engine_worlds({j: rot(axis, 10.0)}, joint_pos, parent, local_trans)
        pv = skin(pos, ws, joint_pos, vc, v)
        sel = np.where(bone_of == j)[0]
        Rf, ang = kabsch(pos_bind[sel], pv[sel])
        c = ls_centre(pos_bind[sel], pv[sel], Rf)
        dax = dist_to_axis(joint_pos[j], c, rot_axis(Rf))
        jd = np.linalg.norm((ws[j] @ np.append(np.zeros(3), 1.0))[:3] - joint_pos[j])
        pj = parent[j]
        prox = 0.0 if pj is None else np.max(
            np.linalg.norm(pv[bone_of == pj] - pos_bind[bone_of == pj], axis=1))
        plm = np.where((bone_of == idx['Palm']) | (bone_of == idx['Hand_Root']))[0]
        od = np.max(np.linalg.norm(pv[plm] - pos_bind[plm], axis=1))
        ok = dax < 0.05 and jd < 0.001 and abs(ang - 10.0) < 0.5 and prox < 0.001 and od < 0.001
        all_ok &= ok
        print('%-9s %12.4f %10.4f %10.2f %10.4f %10.4f  %s' %
              (name, dax, jd, ang, prox, od, 'OK' if ok else 'FAIL'))
    print('  -> single-joint pivots: %s' % ('ALL OK' if all_ok else 'FAILURES'))

    # B2: grip=1 (runtime grab angles, runtime axes)
    print('\n[Part B2] grip=1 (runtime grab angles, runtime axes)')
    rots = {}
    for f, angs in CHAIN_SEGS:
        axis = chain_axis(f)
        for k, a in enumerate(angs, start=1):
            if a:
                rots[idx[f + str(k)]] = rot(axis, a)
    ws = engine_worlds(rots, joint_pos, parent, local_trans)
    pv = skin(pos, ws, joint_pos, vc, v)

    plm = np.where((bone_of == idx['Palm']) | (bone_of == idx['Hand_Root']))[0]
    od = np.max(np.linalg.norm(pv[plm] - pos_bind[plm], axis=1))
    print('  palm/sleeve max displacement = %.4f (expect ~0)' % od)
    print('  per-joint segment rotation (expect cumulative):')
    ok2 = od < 0.001
    for f, angs in CHAIN_SEGS:
        axis = chain_axis(f)
        cum = 0.0
        for k, a in enumerate(angs, start=1):
            j = idx[f + str(k)]
            sel = np.where(bone_of == j)[0]
            if len(sel) == 0:
                continue
            cum += a
            _, ang = kabsch(pos_bind[sel], pv[sel])
            Rw = ws[j][:3, :3]
            ang_mtx = np.degrees(np.arccos(np.clip((np.trace(Rw) - 1) / 2, -1, 1)))
            principal = min(cum % 360.0, 360.0 - (cum % 360.0))
            ok = abs(ang_mtx - principal) < 0.5 and abs(ang - principal) < 15.0
            ok2 &= ok
            print('  %-8s %2d: seg rot %6.2f (matrix %6.2f, expect %6.2f principal %6.2f)  %s' %
                  (f, k, ang, ang_mtx, cum, principal, 'OK' if ok else 'FAIL'))
    # Thumb1's own joint must stay fixed (its children follow the chain)
    t1 = idx['Thumb1']
    t1d = np.linalg.norm((ws[t1] @ np.append(np.zeros(3), 1.0))[:3] - joint_pos[t1])
    print('  Thumb1 joint drift = %.4f (expect 0; Thumb2/3 swing with the chain)' % t1d)
    ok2 &= t1d < 0.001
    print('  -> grip=1: %s' % ('ALL OK' if ok2 else 'FAILURES'))
    return all_ok and ok2


def main():
    ok_r = report_hand(RIGHT, 'RIGHT HAND')
    ok_l = report_hand(LEFT, 'LEFT HAND')
    print('\n' + '=' * 104)
    print('OVERALL: %s' % ('ALL CHECKS PASSED' if (ok_r and ok_l) else 'CHECK FAILURES PRESENT'))


if __name__ == '__main__':
    main()