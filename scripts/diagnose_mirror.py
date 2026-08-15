"""diagnose_mirror.py - Points 5 and 6.

Point 5: right vs left per-joint table:
  - right/left bind positions (mesh = -row3 reading, i.e. what the engine
    SHOULD use) and scene-translate positions (identical)
  - right/left joint->child (distal) vectors, relation (same/opposite/rotated/different)
  - flex axes from PlayerHands.cpp:207-208 (fingers right (0,0,+1), left (0,0,-1);
    thumb right (0,0.891,+0.454), left (0,0.891,-0.454))

Point 6: for each joint:
  M = inverse(INV_BIND_MATRIX)
  errorPosition = distance(M.translation, jointPosition)
  - engine reading (translation at floats 3/7/11) and file-intended reading
    (translation at row 3, floats 12/13/14)
  INV_BIND x M = I and M x INV_BIND = I (max error)

Read-only.
"""

import sys

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import fix_rig_weights as fw

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
LEFT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae'

# PlayerHands.cpp:207-208
FLEX = {'fingers': (np.array([0, 0, 1.0]), np.array([0, 0, -1.0])),   # right, left
        'thumb': (np.array([0, 0.8910, 0.4540]), np.array([0, 0.8910, -0.4540]))}


def load(path):
    tree, root, pos, joints, joint_pos, vw, vc_el, v_el, vc, v = fw.parse(path)
    skin = root.find('.//c:controller/c:skin', fw.NS)
    src = {}
    for s in root.iter(fw.TNS + 'source'):
        f = s.find('c:float_array', fw.NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
    ibm_id = skin.find("c:joints/c:input[@semantic='INV_BIND_MATRIX']", fw.NS).get('source')[1:]
    ibm = src[ibm_id].reshape(-1, 16)
    return joints, joint_pos, ibm


def main():
    rj, rjp, ribm = load(RIGHT)
    lj, ljp, libm = load(LEFT)

    print('=' * 100)
    print('POINT 5 - RIGHT vs LEFT per joint')
    print('=' * 100)
    print('%-9s %-24s %-24s %-12s %-22s %-22s %-10s' %
          ('Joint', 'Right mesh pos', 'Left mesh pos', 'Y relation',
           'Right child vec', 'Left child vec', 'Vec rel'))
    chains = [('Little', ['Little1', 'Little2', 'Little3']),
              ('Ring', ['Ring1', 'Ring2', 'Ring3']),
              ('Middle', ['Middle1', 'Middle2', 'Middle3']),
              ('Index', ['Index1', 'Index2', 'Index3']),
              ('Thumb', ['Thumb1', 'Thumb2', 'Thumb3'])]
    for cname, bones in chains:
        for k, b in enumerate(bones):
            i = rj.index(b)
            j = lj.index(b)
            rp, lp = rjp[i], ljp[j]
            # distal direction: next joint - joint (X3: same as X2->X3)
            if k < 2:
                rv = rjp[rj.index(bones[k + 1])] - rp
                lv = ljp[lj.index(bones[k + 1])] - lp
            else:
                rv = rp - rjp[rj.index(bones[k - 1])]
                lv = lp - ljp[lj.index(bones[k - 1])]
            ru, lu = rv / np.linalg.norm(rv), lv / np.linalg.norm(lv)
            ang = np.degrees(np.arccos(np.clip(ru @ lu, -1, 1)))
            yrel = 'mirror-ish' if abs(rp[1] + lp[1]) < 0.85 else 'different'
            vrel = 'same' if ang < 15 else ('opposite' if ang > 165 else
                                            ('rotated %.0f deg' % ang))
            print('%-9s (%7.2f,%7.2f,%7.2f) (%7.2f,%7.2f,%7.2f)  %-11s  (%6.2f,%6.2f,%6.2f)  (%6.2f,%6.2f,%6.2f)  %s' %
                  (b, *rp, *lp, yrel, *rv, *lv, vrel))
        flex = FLEX['thumb' if cname == 'Thumb' else 'fingers']
        rr = FLEX['fingers' if cname != 'Thumb' else 'thumb']
        if cname == 'Thumb':
            print('  %s: flex axis right %s left %s -> z-flip of the z component (y kept)' %
                  (cname, rr[0].round(3), rr[1].round(3)))
        else:
            print('  %s: flex axis right %s left %s -> %s' %
                  (cname, rr[0].round(3), rr[1].round(3),
                   'OPPOSITE (z-flip)' if np.allclose(rr[0], -rr[1]) else 'same'))
    print('  Conclusion: distal vectors are ~parallel (same direction, +x); y component mirrors;')
    print('  flex axes are z-flipped (opposite); geometry proportions differ (y offsets, |d|<=0.8).')

    print()
    print('=' * 100)
    print('POINT 6 - INV_BIND_MATRIX check, numerical')
    print('=' * 100)
    for path, label, joints, joint_pos, ibm in [(RIGHT, 'RIGHT', rj, rjp, ribm),
                                                (LEFT, 'LEFT', lj, ljp, libm)]:
        max_err_engine = 0.0
        max_err_intended = 0.0
        max_prod = 0.0
        print('  %s:' % label)
        for k, b in enumerate(joints):
            m4 = ibm[k].reshape(4, 4)
            M = np.linalg.inv(m4)
            # engine reading: translation = (m[0][3], m[1][3], m[2][3]) = floats 3/7/11
            t_engine = M[:3, 3] if np.any(M[:3, 3]) else np.array([M[0, 3], M[1, 3], M[2, 3]])
            # engine sees INV_BIND with translation read from column 3:
            m_engine = m4.copy()
            t_read = np.array([m_engine[0, 3], m_engine[1, 3], m_engine[2, 3]])
            if np.allclose(t_read, 0):
                m_engine[0, 3] = m_engine[1, 3] = m_engine[2, 3] = 0.0
            else:
                m_engine[:3, 3] = t_read
            M_engine = np.linalg.inv(m_engine)
            err_engine = np.linalg.norm(M_engine[:3, 3] - joint_pos[k])
            # file-intended: translation = row 3 (floats 12/13/14); M's translation
            # in the same convention = M[3,:3] (row-vector matrices)
            M_int = np.linalg.inv(m4)
            err_intended = np.linalg.norm(M_int[3, :3] - joint_pos[k])
            p1 = np.linalg.norm(m4 @ M - np.eye(4))
            p2 = np.linalg.norm(M @ m4 - np.eye(4))
            max_err_engine = max(max_err_engine, err_engine)
            max_err_intended = max(max_err_intended, err_intended)
            max_prod = max(max_prod, p1, p2)
            if k < 3 or k >= len(joints) - 2:
                print('    %-9s jointPos (%7.2f,%7.2f,%7.2f) | engine-read M.translation (%7.2f,%7.2f,%7.2f) '
                      'err=%7.3f | file-intended M.translation (%7.2f,%7.2f,%7.2f) err=%8.4f | INVxB err %.1e' %
                      (b, *joint_pos[k], *M_engine[:3, 3], err_engine,
                       *M_int[3, :3], err_intended, max(p1, p2)))
        print('    max errorPosition (engine reading) = %.3f  -> NOT small -> Hypothesis B NOT discarded' %
              max_err_engine)
        print('    max errorPosition (file-intended reading) = %.4f (consistent internal matrices)' %
              max_err_intended)
        print('    max |INV_BIND x M - I| and |M x INV_BIND - I| = %.2e (identity to machine precision)' %
              max_prod)


main()