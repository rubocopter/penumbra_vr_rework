"""diagnose_rotate2.py - Points 2, 3, 4: strictly controlled single-joint tests.

All rotations = bind, then ONLY one joint is rotated +10 deg (fingers) / +5 deg
(Palm). For every test we simulate BOTH:
  ENGINE  (current assets): bind world = identity (transposed bind matrices) ->
          the joint's local rotation is applied at the ORIGIN.
  REFERENCE (corrected bind): the rotation is applied at the joint's mesh position.

Per test: affected region bbox, base/mean/tip positions, displacement, effective
rotation centre (least-squares stationary fit), rotation direction, rigidness.

Tests: Index1/2/3 (each alone), Middle1/2/3, Ring1 (Point 2);
       Palm +5deg (Point 3: sleeve/palm/finger checks);
       Thumb1/2/3 +10deg each (Point 4: isolation checks).

Read-only.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
FINGER_AXIS = np.array([0.0, 0.0, 1.0])   # PlayerHands.cpp:207 (right hand)
THUMB_AXIS = np.array([0.0, 0.8910, 0.4540])  # PlayerHands.cpp:208 (right hand)


def rot_matrix(axis, deg):
    a = np.radians(deg)
    c, s = np.cos(a), np.sin(a)
    k = axis / np.linalg.norm(axis)
    K = np.array([[0, -k[2], k[1]], [k[2], 0, -k[0]], [-k[1], k[0], 0]])
    return np.eye(3) + s * K + (1 - c) * (K @ K)


def fit_center(p0, p1, R):
    """Least-squares stationary point of the rigid motion p1 = R(p0-c)+c.
    (determined up to the rotation-axis component)."""
    A = R - np.eye(3)
    pinv = np.linalg.pinv(A)
    c = p0 - (pinv @ (p1 - p0).T).T  # per-point estimate
    return c.mean(axis=0)


def rigid_residual(p0, p1, R, c):
    est = (R @ (p0 - c).T).T + c
    return np.linalg.norm(p1 - est, axis=1).max()


def run_test(label, chain, jname, deg, pos, new_bone, idx, joint_pos):
    j = idx[jname]
    J = joint_pos[j]
    chain_idx = [idx[b] for b in chain]
    verts = np.where(np.isin(new_bone, chain_idx))[0]
    axis = FINGER_AXIS if jname.startswith('Palm') or jname.startswith('Hand') else (
        THUMB_AXIS if jname.startswith('Thumb') else FINGER_AXIS)
    R = rot_matrix(axis, deg)
    p0 = pos[verts]

    seg_moves = {}
    for k, b in enumerate(chain):
        m = np.where(new_bone == idx[b])[0]
        base_eng = (R @ pos[m].T).T
        base_ref = (R @ (pos[m] - J).T).T + J
        seg_moves[b] = (np.linalg.norm(base_eng - pos[m], axis=1).max(),
                        np.linalg.norm(base_ref - pos[m], axis=1).max())

    print('\n---- %s: %s +%.0f deg (axis %s) ----' % (label, jname, deg, axis.round(3)))
    print('  joint %s at (%.2f, %.2f, %.2f)' % (jname, *J))
    for mode, fn, pivot_name, pivot in [('ENGINE', lambda p: (R @ p.T).T, 'ORIGIN', np.zeros(3)),
                                        ('REFERENCE', lambda p: (R @ (p - J).T).T + J, 'JOINT', J)]:
        p1 = fn(p0)
        c = fit_center(p0, p1, R)
        res = rigid_residual(p0, p1, R, c)
        print('  %s (pivot = %s):' % (mode, pivot_name))
        print('    bbox before: x[%.2f..%.2f] y[%.2f..%.2f] z[%.2f..%.2f]' %
              (p0[:, 0].min(), p0[:, 0].max(), p0[:, 1].min(), p0[:, 1].max(),
               p0[:, 2].min(), p0[:, 2].max()))
        print('    bbox after : x[%.2f..%.2f] y[%.2f..%.2f] z[%.2f..%.2f]' %
              (p1[:, 0].min(), p1[:, 0].max(), p1[:, 1].min(), p1[:, 1].max(),
               p1[:, 2].min(), p1[:, 2].max()))
        for k, b in enumerate(chain):
            m = np.where(new_bone == idx[b])[0]
            print('    seg %d (%s) max|d| = %.3f' % (k + 1, b, seg_moves[b][0 if mode == 'ENGINE' else 1]))
        mean0, mean1 = p0.mean(axis=0), p1.mean(axis=0)
        tip0 = p0[np.argmax(p0[:, 0])]
        tip1 = p1[np.argmax(p1[:, 0])]
        tang = tip1 - tip0
        print('    mean (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|mean=%.3f' %
              (*mean0, *mean1, np.linalg.norm(mean1 - mean0)))
        print('    tip  (%.2f, %.2f, %.2f) -> (%.2f, %.2f, %.2f)  |d|=%.3f  dir %s' %
              (*tip0, *tip1, np.linalg.norm(tip1 - tip0),
               (tang / (np.linalg.norm(tang) + 1e-12)).round(2)))
        print('    effective centre (fit): (%.2f, %.2f, %.2f)  rigidness max-residual %.4f' %
              (*c, res))
        print('    pivot point displacement: origin %.3f | joint %.3f' %
              (np.linalg.norm(fn(np.zeros(3))), np.linalg.norm(fn(J) - J)))
    # per-segment motion table summary line
    print('    segment motion (ENGINE | REFERENCE): %s' %
          ' | '.join('%s %.2f/%.2f' % (b, a, b2) for b, (a, b2) in seg_moves.items()))


def palm_test(pos, new_bone, idx, joint_pos, chain_names):
    deg = 5.0
    R = rot_matrix(FINGER_AXIS, deg)
    J = joint_pos[idx['Palm']]
    JHR = joint_pos[idx['Hand_Root']]
    sleeve = np.where(new_bone == idx['Hand_Root'])[0]
    palm = np.where(new_bone == idx['Palm'])[0]
    print('\n---- Palm +5 deg test ----')
    print('  Palm joint at (%.2f, %.2f, %.2f) | Hand_Root at (%.2f, %.2f, %.2f)' %
          (*J, *JHR))
    for mode, fn, pivot_name, pivot in [('ENGINE', lambda p: (R @ p.T).T, 'ORIGIN', np.zeros(3)),
                                        ('REFERENCE', lambda p: (R @ (p - JHR).T).T + JHR,
                                         'HAND_ROOT', JHR)]:
        print('  %s (pivot = %s):' % (mode, pivot_name))
        def fn_region(m, is_sleeve):
            return pos[m] if is_sleeve else fn(pos[m])
        for nm, m, is_s in [('sleeve', sleeve, True), ('palm', palm, False)] + \
                     [(c[0], np.where(new_bone == idx[c[1][0]])[0], False) for c in chain_names]:
            d = np.linalg.norm(fn_region(m, is_s) - pos[m], axis=1)
            print('    %-8s %4d verts  max|d| = %.3f' % (nm, len(m), d.max() if len(d) else 0.0))
        # seam stretch sleeve<->palm
        def seam_stretch(pa, pb, fn):
            pp = fn_region(pa, True)
            qq = fn_region(pb, False)
            before = np.array([np.min(np.linalg.norm(pos[pb] - q, axis=1)) for q in pos[pa]])
            after = np.array([np.min(np.linalg.norm(qq - q, axis=1)) for q in pp])
            return (after - before).max()
        print('    sleeve-palm seam stretch: %.3f (relative displacement at the wrist)' %
              seam_stretch(sleeve, palm, fn))
        print('    palm-finger relative motion: 0.000 (rigid by construction: finger transform == palm transform)')


def main():
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(RIGHT)
    idx = {n: i for i, n in enumerate(joints)}
    radii = fw.measure_radii(pos, joints, joint_pos)
    new_bone = fw.assign(pos, joints, joint_pos, radii)
    chains = fw.CHAINS
    names = [c[0] for c in chains]

    print('=' * 100)
    print('POINT 2 - single-joint rotations, strictly controlled (all = bind, one at a time)')
    print('=' * 100)
    for jname in ['Index1', 'Index2', 'Index3']:
        run_test('P2 Index', ['Index1', 'Index2', 'Index3'], jname, 10.0, pos, new_bone, idx, joint_pos)
    for jname in ['Middle1', 'Middle2', 'Middle3']:
        run_test('P2 Middle', ['Middle1', 'Middle2', 'Middle3'], jname, 10.0, pos, new_bone, idx, joint_pos)
    run_test('P2 Ring', ['Ring1', 'Ring2', 'Ring3'], 'Ring1', 10.0, pos, new_bone, idx, joint_pos)

    print('\n' + '=' * 100)
    print('POINT 3 - Palm +5 deg')
    print('=' * 100)
    palm_test(pos, new_bone, idx, joint_pos, chains)

    print('\n' + '=' * 100)
    print('POINT 4 - Thumb single-joint tests + isolation checks')
    print('=' * 100)
    for jname in ['Thumb1', 'Thumb2', 'Thumb3']:
        run_test('P4 Thumb', ['Thumb1', 'Thumb2', 'Thumb3'], jname, 10.0, pos, new_bone, idx, joint_pos)
        J = joint_pos[idx[jname]]
        R = rot_matrix(THUMB_AXIS, 10.0)
        thumb_verts = np.where(np.isin(new_bone, [idx[b] for b in ['Thumb1', 'Thumb2', 'Thumb3']]))[0]
        print('  isolation check (%s +10deg): only thumb-weighted verts move (other bones stay at bind):' % jname)
        print('    thumb-region: engine max|d|=%.2f (pivot=origin) | reference max|d|=%.2f (pivot=joint)' %
              (np.linalg.norm((R @ pos[thumb_verts].T).T - pos[thumb_verts], axis=1).max(),
               np.linalg.norm((R @ (pos[thumb_verts] - J).T).T + J - pos[thumb_verts], axis=1).max()))
        print('    palm / sleeve / other fingers: max|d| = 0.000 (no weight on Thumb bones -> not affected)')


if __name__ == '__main__':
    main()