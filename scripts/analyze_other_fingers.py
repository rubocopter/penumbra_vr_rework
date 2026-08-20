"""Geometric analysis of the flexion axes for Index, Ring, Little and Thumb,
derived from the bind geometry of hud_object_hand_rig.dae.

For each chain we compute the anatomical vertical fold axis
(axis = normalize(cross(proximal dir, -Y)), so the finger folds straight down
in its own sagittal plane, like a real finger) and compare it with the
currently assigned axis. Then we simulate tip trajectories (palm frame) with
both, and search reduced fold tables for Little and Index so their tips stay
in their own lateral zone (no crossing under the adjacent finger) while still
reaching the palm depth. The thumb is analysed separately (short chain).

Analysis only - no game files are modified.
"""

import sys
import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import diag_rig_geometry as dg

GRAB = {
    'Little1': 30, 'Little2': 30, 'Little3': 20,
    'Index1': 0, 'Index2': 0, 'Index3': 0,
    'Ring1': 17, 'Ring2': 17, 'Ring3': 10,
    'Middle1': 17, 'Middle2': 17, 'Middle3': 10,
    'Thumb1': 130, 'Thumb2': 90, 'Thumb3': 0,
}
TRIGGER = {
    'Index1': 17, 'Index2': 17, 'Index3': 10,
}

CHAINS = {
    'Middle': ('Middle1', 'Middle2', 'Middle3'),
    'Index': ('Index1', 'Index2', 'Index3'),
    'Ring': ('Ring1', 'Ring2', 'Ring3'),
    'Little': ('Little1', 'Little2', 'Little3'),
    'Thumb': ('Thumb1', 'Thumb2', 'Thumb3'),
}

CURRENT_AXIS = {
    'Middle': np.array([0.0, 0.0, -1.0]),
    'Index': np.array([0.0, 0.0, -1.0]),
    'Ring': np.array([0.0, 0.0, -1.0]),
    'Little': np.array([0.0, -0.8660, -0.5000]),
    'Thumb': np.array([0.065, 0.9970, -0.0460]),
}

PALM_FACE_Y = -2.2


def norm(v):
    return v / (np.linalg.norm(v) + 1e-12)


def main():
    tree, root, pos, joints, joint_pos, local_trans, vc, v, ibm = dg.parse(dg.PATH)
    idx = {n: i for i, n in enumerate(joints)}
    n = len(joints)

    parent = {idx['Hand_Root']: None}
    vs = root.find('.//c:visual_scene', dg.fw.NS)
    def walk(node, p):
        name = node.get('name')
        if name in idx:
            parent[idx[name]] = p
        for ch in list(node):
            if ch.tag == dg.fw.TNS + 'node':
                walk(ch, idx.get(name))
    walk(vs, None)

    def pose(deg_by_joint, axes_override=None):
        ws = []
        for i in range(n):
            name = joints[i]
            deg = deg_by_joint.get(name, 0.0)
            ax = None
            if axes_override is not None:
                ch = name.rstrip('0123')
                ax = axes_override.get(ch)
            if ax is None:
                ax = dg.axis_for(name)
            R = dg.rmat(ax, deg) if deg else np.eye(3)
            L = np.eye(4)
            L[:3, :3] = R
            L[:3, 3] = local_trans[i]
            ws.append(L if parent[i] is None else ws[parent[i]] @ L)
        return ws

    palm_pos = joint_pos[idx['Palm']]

    def tip_rel(deg_by_joint, bone, axes_override=None):
        ws = pose(deg_by_joint, axes_override)
        tip = (ws[idx[bone]] @ np.array([0.0, 0.0, 0.0, 1.0]))[:3]
        return tip - palm_pos

    def fold(dict_angles, grip):
        return {k: grip * a for k, a in dict_angles.items()}

    print('=== 1. bind chains: segments, direction, anatomical vertical axis ===')
    ax_vert = {}
    for finger, bones in CHAINS.items():
        j1 = joint_pos[idx[bones[0]]] - palm_pos
        j2 = joint_pos[idx[bones[1]]] - palm_pos
        j3 = joint_pos[idx[bones[2]]] - palm_pos
        s1, s2 = j2 - j1, j3 - j2
        d = norm(s1)
        av = norm(np.cross(d, np.array([0.0, -1.0, 0.0])))
        r_tip = j1 + s1 + s2
        if np.cross(av, r_tip - j1)[1] > 0:
            av = -av
        ax_vert[finger] = av
        cur = CURRENT_AXIS[finger]
        ang = np.degrees(np.arccos(np.clip(np.dot(av, cur), -1, 1)))
        print('  %-6s d=%s |s1|=%.3f |s2|=%.3f  vertical axis=%s'
              '  angle(current,vertical)=%.1f deg'
              % (finger, np.round(d, 3), np.linalg.norm(s1), np.linalg.norm(s2),
                 np.round(av, 3), ang))

    def show(finger, bones, angles, label, axes=None, grips=(0.5, 0.7, 1.0)):
        ax_txt = ''
        if axes is not None and finger in axes:
            ax_txt = ' axis=' + str(np.round(np.asarray(axes[finger]), 3))
        print('\n  %s %s (angles %s%s):'
              % (finger, label, angles, ax_txt))
        tb = tip_rel({}, bones[2])
        for g in grips:
            t = tip_rel(fold(angles, g), bones[2], axes_override=axes)
            print('    g=%.1f tip=%s  (z-drift %+5.3f, y %+6.3f)'
                  % (g, np.round(t, 3), t[2] - tb[2], t[1] - tb[1]))

    print('\n=== 2. current axis + current angles ===')
    show('Little', CHAINS['Little'], GRAB, 'grab', None)
    show('Index', CHAINS['Index'], TRIGGER, 'trigger', None)
    show('Ring', CHAINS['Ring'], GRAB, 'grab', None)
    show('Middle', CHAINS['Middle'], GRAB, 'grab', None)
    show('Thumb', CHAINS['Thumb'], GRAB, 'grab', None)

    print('\n=== 3. anatomical vertical axis + current angles ===')
    show('Little', CHAINS['Little'], GRAB, 'grab', {'Little': ax_vert['Little']})
    show('Index', CHAINS['Index'], TRIGGER, 'trigger', {'Index': ax_vert['Index']})
    show('Ring', CHAINS['Ring'], GRAB, 'grab', {'Ring': ax_vert['Ring']})
    show('Middle', CHAINS['Middle'], GRAB, 'grab', {'Middle': ax_vert['Middle']})
    show('Thumb', CHAINS['Thumb'], GRAB, 'grab', {'Thumb': ax_vert['Thumb']})

    print('\n=== 4. palm flesh z-extent (where does the palm face actually exist) ===')
    psel = [i for i in range(len(vc)) if int(v[2 * i]) == idx['Palm']]
    pv = pos[psel] - palm_pos
    for z0 in np.arange(-6.0, 8.5, 1.0):
        band = pv[(np.abs(pv[:, 2] - z0) < 0.5) & (pv[:, 0] >= -2.0) & (pv[:, 0] <= 3.0)]
        if len(band) == 0:
            print('  z=%+4.1f: --' % z0)
            continue
        ys = np.sort(band[:, 1])
        print('  z=%+4.1f: %2d verts  y med=%.2f (min %.2f max %.2f)'
              % (z0, len(band), ys[len(ys) // 2], ys[0], ys[-1]))

    print('\n=== 5. Little: search (tilted axis kept) with the tip staying')
    print('        in the pinky zone (z <= -1.2) and at palm depth at g0.7 ===')
    print('  ring base z = %+.3f' % (joint_pos[idx['Ring1']][2] - palm_pos[2]))
    l1, l2, l3 = CHAINS['Little']
    best = []
    for A1 in range(10, 61):
        for A2 in range(10, 61):
            if A2 > A1:
                continue
            for A3 in range(0, A2 + 1):
                t07 = tip_rel(fold({'Little1': A1, 'Little2': A2, 'Little3': A3}, 0.7),
                              l3, {'Little': CURRENT_AXIS['Little']})
                if not (-4.2 <= t07[1] <= -2.4) or not (t07[2] <= -1.2):
                    continue
                cost = 2 * abs(A1 - 55) + 2 * abs(A2 - 55) + abs(A3 - 40)
                best.append((cost, A1, A2, A3))
    best.sort()
    print('  top-6 (cost | A1 A2 A3 | tip at g0.5/0.6/0.7/0.8/1.0):')
    for cost, A1, A2, A3 in best[:6]:
        row = []
        for g in (0.5, 0.6, 0.7, 0.8, 1.0):
            t = tip_rel(fold({'Little1': A1, 'Little2': A2, 'Little3': A3}, g),
                        l3, {'Little': CURRENT_AXIS['Little']})
            row.append('g%.1f=(%.2f,%.2f,z%.2f)' % (g, t[0], t[1], t[2]))
        print('    cost=%3d | %2d %2d %2d | %s' % (cost, A1, A2, A3, '  '.join(row)))

    print('\n=== 6. Index (trigger): search with the current axis (0,0,-1) ===')
    i1, i2, i3 = CHAINS['Index']
    best = []
    for A1 in range(10, 61):
        for A2 in range(10, 61):
            if A2 > A1:
                continue
            for A3 in range(0, A2 + 1):
                t07 = tip_rel(fold({'Index1': A1, 'Index2': A2, 'Index3': A3}, 0.7),
                              i3, {'Index': CURRENT_AXIS['Index']})
                if not (-4.2 <= t07[1] <= -2.4):
                    continue
                cost = 2 * abs(A1 - 55) + 2 * abs(A2 - 55) + abs(A3 - 45)
                best.append((cost, A1, A2, A3))
    best.sort()
    print('  top-6 (cost | A1 A2 A3 | tip at g0.5/0.6/0.7/1.0):')
    for cost, A1, A2, A3 in best[:6]:
        row = []
        for g in (0.5, 0.6, 0.7, 1.0):
            t = tip_rel(fold({'Index1': A1, 'Index2': A2, 'Index3': A3}, g),
                        i3, {'Index': CURRENT_AXIS['Index']})
            row.append('g%.1f=(%.2f,%.2f,z%.2f)' % (g, t[0], t[1], t[2]))
        print('    cost=%3d | %2d %2d %2d | %s' % (cost, A1, A2, A3, '  '.join(row)))

    print('\n=== 7. Thumb: what CAN the thumb reach? (short chain, far out at z=11-12) ===')
    t1, t2, t3 = CHAINS['Thumb']
    j1 = joint_pos[idx[t1]] - palm_pos
    print('  thumb base = %s  chain = %.3f' % (np.round(j1, 3), 3.502))
    print('  max reach from base: %.3f  ->  the tip cannot reach the palm flesh'
          % (3.502 + 0.5))
    print('  fold planes:')
    print('    current axis (0,0.982,0.187): fold plane ~horizontal (X-Z), tip sweeps')
    print('      sideways and backward at y ~ -1.0..-1.8 (never descends to palm face)')
    av = ax_vert['Thumb']
    print('    vertical axis %s: fold plane X-Y, tip descends toward the palm' %
          np.round(av, 3))
    for label, ax in (('current', CURRENT_AXIS['Thumb']), ('vertical', av)):
        row = []
        for g in (0.3, 0.5, 0.6, 0.7, 0.8, 1.0):
            t = tip_rel(fold(GRAB, g), t3, {'Thumb': ax})
            tb = tip_rel({}, t3)
            row.append('g%.1f=(%.1f,%.1f,%.1f)|%.1f'
                       % (g, t[0], t[1], t[2], np.linalg.norm(t - tb)))
        print('  %-8s: %s' % (label, '  '.join(row)))

    print('\n=== 8. Thumb: vertical axis + reduced angles (no deep dive) ===')
    for A1, A2 in ((130, 90), (90, 50), (70, 40), (55, 30)):
        row = []
        for g in (0.5, 0.6, 0.7, 0.8, 1.0):
            t = tip_rel(fold({'Thumb1': A1, 'Thumb2': A2, 'Thumb3': 0}, g),
                        t3, {'Thumb': ax_vert['Thumb']})
            row.append('g%.1f=(%.1f,%.1f,%.1f)' % (g, t[0], t[1], t[2]))
        print('  %3d/%2d: %s' % (A1, A2, '  '.join(row)))
    print('  note: Thumb3 rotation does not move the Thumb3 joint origin (tip probe'
          ' is the joint); only the skin pad beyond it would curl (~0.3 u).')


if __name__ == '__main__':
    main()
