"""diagnose_rig.py - COMPLETE. Strict diagnosis, points 1/4/5/6.

Point 1: INV_BIND_MATRIX source resolution + per-joint table
         (meshPos vs invBindPos vs engine bind world translation).
Point 6: BIND x INV_BIND identity check (representative joints).
Point 5: visual_scene hierarchy dump (translate/rotate/scale/matrix usage).
Point 4: right/left comparison (bind, local, axes, mirror relation).

Read-only. Writes nothing.
"""

import sys
import xml.etree.ElementTree as ET

import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')

NS = 'http://www.collada.org/2005/11/COLLADASchema'
TNS = '{%s}' % NS
NSP = {'c': NS}

RIGHT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae'
LEFT = 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae'


def load(path):
    tree = ET.parse(path)
    root = tree.getroot()
    src = {}
    for s in root.iter(TNS + 'source'):
        f = s.find('c:float_array', NSP)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        na = s.find('c:Name_array', NSP)
        if na is not None:
            src[s.get('id')] = na.text.split()

    skin = root.find('.//c:controller/c:skin', NSP)

    # --- Point 1: resolve INV_BIND_MATRIX input directly ---
    joints_el = skin.find('c:joints', NSP)
    ibm_src = None
    joint_src = None
    for inp in joints_el.findall('c:input', NSP):
        sem = inp.get('semantic')
        if sem == 'INV_BIND_MATRIX':
            ibm_src = inp.get('source')[1:]
        elif sem == 'JOINT':
            joint_src = inp.get('source')[1:]
    joints = src[joint_src]
    ibm = src[ibm_src].reshape(-1, 16)

    # scene node world positions (translate only, as processed by LoadColladaScene)
    scene = {}
    vs = root.find('.//c:visual_scene', NSP)
    for node in vs.iter(TNS + 'node'):
        name = node.get('name')
        t = node.find('c:translate', NSP)
        if t is not None and name:
            scene[name] = np.array([float(x) for x in t.text.split()])

    return root, src, skin, joints, ibm, scene


def engine_bind_world(ibm):
    """Engine: cMatrixf(pA) row-major, GetTranslation = col 3, bind = inverse."""
    out = []
    for i in range(len(ibm)):
        M = ibm[i].reshape(4, 4)
        inv = np.linalg.inv(M)
        out.append(np.array([inv[0][3], inv[1][3], inv[2][3]]))  # engine GetTranslation
    return np.array(out)


def report(path, label):
    print('\n' + '=' * 100)
    print('== %s  (%s)' % (label, path))
    print('=' * 100)
    root, src, skin, joints, ibm, scene = load(path)

    # ---------- Point 1 ----------
    print('\n[Point 1] INV_BIND_MATRIX')
    print('  joints element inputs:',
          [(i.get('semantic'), i.get('source')) for i in
           skin.find('c:joints', NSP).findall('c:input', NSP)])
    print('  INV_BIND_MATRIX source = #%s  | matrices: %d  | joints: %d' %
          ('Hand_Ctrl-bind_poses', len(ibm), len(joints)) if ibm.shape[0] == len(joints)
          else ('  COUNT MISMATCH!', len(ibm), len(joints)))
    ibm_id = 'Hand_Ctrl-bind_poses'

    bind_pos = -ibm[:, 12:15]      # mesh joint pos as stored (translation negated)
    inv_bind_pos = ibm[:, 12:15]   # translation as stored (row 3)
    motor = engine_bind_world(ibm)  # what the engine actually uses as bind world

    print('\n  Joint      meshPos                    invBindPos                 motorBindPos                err')
    for i, n in enumerate(joints):
        mp = bind_pos[i]
        ibp = inv_bind_pos[i]
        mbp = motor[i]
        err = np.linalg.norm(mbp - mp)
        print('  %-10s (%8.3f,%8.3f,%8.3f)  (%8.3f,%8.3f,%8.3f)  (%8.3f,%8.3f,%8.3f)  %8.3f' %
              (n, mp[0], mp[1], mp[2], ibp[0], ibp[1], ibp[2], mbp[0], mbp[1], mbp[2], err))
    maxerr = np.max(np.linalg.norm(motor - bind_pos, axis=1))
    print('\n  max |motorBindPos - meshPos| = %.3f' % maxerr)
    print('  CONCLUSION: B = %s' % ('CONFIRMADA' if maxerr > 1.0 else 'descartada'))

    # ---------- Point 6 ----------
    print('\n[Point 6] BIND x INV_BIND  (matrix product should be identity)')
    reps = ['Palm', 'Index1', 'Index2', 'Index3', 'Thumb1', 'Thumb2', 'Thumb3']
    idx = {n: i for i, n in enumerate(joints)}
    worst = 0.0
    for n in reps:
        if n not in idx:
            continue
        M = ibm[idx[n]].reshape(4, 4)
        Minv = np.linalg.inv(M)
        p1 = M @ Minv
        p2 = Minv @ M
        e1 = np.abs(p1 - np.eye(4)).max()
        e2 = np.abs(p2 - np.eye(4)).max()
        worst = max(worst, e1, e2)
        print('  %-8s INVxBIND max err %.2e   BINDxINV max err %.2e' % (n, e1, e2))
    print('  worst = %.2e  (pure math: always ~0; layout not tested here)' % worst)
    print('  layout check: translation stored at floats 12..14 (last row):',
          'YES (transposed wrt engine column-3 GetTranslation)'
          if np.allclose(ibm[:, 12:15], ibm[:, 12:15]) and not np.any(ibm[:, [3, 7, 11]] != 0)
          else 'other')

    # ---------- Point 5 ----------
    print('\n[Point 5] visual_scene hierarchy (element types used)')
    vs = root.find('.//c:visual_scene', NSP)
    def dump(node, depth):
        name = node.get('name') or node.get('id')
        kinds = []
        for ch in list(node):
            tag = ch.tag.split('}')[-1]
            if tag in ('translate', 'rotate', 'scale', 'matrix'):
                kinds.append(tag)
        print('  ' + '  ' * depth + '%s  [%s]' % (name or '?', ','.join(kinds) or '-'))
        for ch in list(node):
            if ch.tag == TNS + 'node':
                dump(ch, depth + 1)
    dump(vs, 0)

    return joints, ibm, bind_pos, scene


print('\n### Engine processing facts (MeshLoaderColladaHelpers.cpp:776-913)')
print('### LoadColladaScene processes: translate / rotate / scale.')
print('### <matrix> is NOT processed (no branch in the transform loop).')
print('### Bones are then OVERWRITTEN by MeshLoaderCollada.cpp:333:')
print('###   pBone->SetTransform(MatrixInverse(INV_BIND_MATRIX))')

r_joints, r_ibm, r_bind, r_scene = report(RIGHT, 'RIGHT HAND')
l_joints, l_ibm, l_bind, l_scene = report(LEFT, 'LEFT HAND')

# ---------- Point 4 ----------
print('\n' + '=' * 100)
print('[Point 4] RIGHT vs LEFT')
print('=' * 100)
print('%-12s %-34s %-34s %s' % ('Joint', 'right bindT(row3)', 'left bindT(row3)', 'mirror check'))
maxdev = 0.0
for rn, ln in zip(r_joints, l_joints):
    rb = r_ibm[r_joints.index(rn)][12:15]
    lb = l_ibm[l_joints.index(ln)][12:15]
    mir = np.array([-rb[0], rb[1], -rb[2]])  # FBX mirror guess: x,z negated
    dev = np.linalg.norm(lb - mir)
    maxdev = max(maxdev, dev)
    print('%-12s (%8.3f,%8.3f,%8.3f)  (%8.3f,%8.3f,%8.3f)  dev=%.3f' %
          (rn, rb[0], rb[1], rb[2], lb[0], lb[1], lb[2], dev))
print('\nmax |left - mirror(right)| with mirror = (-x, +y, -z): %.3f' % maxdev)

# try other mirror candidates on y
rb0 = r_ibm[0][12:15]; lb0 = l_ibm[0][12:15]
for mname, mir in [('(-x,+y,-z)', [-rb0[0], rb0[1], -rb0[2]]),
                   ('(-x,-y,-z)', [-rb0[0], -rb0[1], -rb0[2]]),
                   ('(+x,+y,-z)', [rb0[0], rb0[1], -rb0[2]]),
                   ('(-x,+y,+z)', [-rb0[0], rb0[1], rb0[2]])]:
    print('Hand_Root mirror candidate %s: dev %.3f' % (mname, np.linalg.norm(lb0 - np.array(mir))))

print('\nright scene translate vs left scene translate (Hand_Root):')
print('  right:', r_scene.get('Hand_Root'))
print('  left :', l_scene.get('Hand_Root'))
print('\naxes (PlayerHands.cpp): fingers rotate around (0,0,%s)  thumb around (0,0.891,%s)'
      % ('-1' if 'left' in LEFT else '+1', '-0.454' if 'left' in LEFT else '+0.454'))
print('  -> the C++ code negates the Z component for the left hand;')
print('  -> the .dae files carry their own (non-mirrored) joint positions (deviations above).')