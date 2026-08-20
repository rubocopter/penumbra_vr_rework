"""Search Middle/Ring fold tables (per-joint angles at grip=1) that keep the
fingertips near the palm zone during the real grip range (0.5-0.7) without
plunging ~10 units below the palm joint. Keeps current axes and linear grip
interpolation. Analysis only - no game files are modified.

Method: engine-exact skinning simulation (validated against the tipProbe VR
capture), palm-relative tip positions in the model frame.
"""

import sys
import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import diag_rig_geometry as dg

TAB = {'Middle1': 70, 'Middle2': 70, 'Middle3': 35,
       'Ring1': 70, 'Ring2': 70, 'Ring3': 35}

TIP_OFFSET = {
    'Middle3': np.array([0.794, 0.016, -0.180]),
    'Ring3': np.array([0.949, -0.025, 0.162]),
}

BONES = {'Middle': ('Middle1', 'Middle2', 'Middle3'),
         'Ring': ('Ring1', 'Ring2', 'Ring3')}

# captured probe: (applied angles (a,a,b) in deg) -> tipPalmLocal
PROBE = {
    'Middle3': [
        ((3.1, 3.1, 1.6), (10.124, -0.911, 1.128)),
        ((21.7, 21.7, 10.9), (8.259, -6.124, 1.128)),
        ((39.9, 39.9, 20.0), (4.228, -9.441, 1.128)),
        ((50.6, 50.6, 25.3), (1.401, -10.200, 1.128)),
        ((53.9, 53.9, 27.0), (0.506, -10.248, 1.128)),
    ],
    'Ring3': [
        ((3.1, 3.1, 1.6), (10.240, -0.919, 1.091)),
        ((21.7, 21.7, 10.9), (8.082, -6.104, 1.091)),
        ((39.9, 39.9, 20.0), (3.812, -9.228, 1.091)),
        ((50.6, 50.6, 25.3), (0.908, -9.817, 1.091)),
        ((53.9, 53.9, 27.0), (-0.001, -9.807, 1.091)),
    ],
}


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

    def pose(deg_by_joint):
        ws = []
        for i in range(n):
            name = joints[i]
            deg = deg_by_joint.get(name, 0.0)
            R = dg.rmat(dg.axis_for(name), deg) if deg else np.eye(3)
            L = np.eye(4)
            L[:3, :3] = R
            L[:3, 3] = local_trans[i]
            ws.append(L if parent[i] is None else ws[parent[i]] @ L)
        return ws

    palm_pos = joint_pos[idx['Palm']]

    def tip_palm_local(deg_by_joint, bone):
        ws = pose(deg_by_joint)
        tip = (ws[idx[bone]] @ np.append(TIP_OFFSET[bone], 1.0))[:3]
        return tip - palm_pos

    def fold(finger, grip, A1, A2, A3):
        b1, b2, b3 = BONES[finger]
        return {b1: grip * A1, b2: grip * A2, b3: grip * A3}

    print('=== 1. sim vs probe validation (engine axes, palm frame) ===')
    for bone, samples in PROBE.items():
        finger = bone[:6] if bone.startswith('Middle') else bone[:4]
        b1, b2, b3 = BONES[finger]
        for (a, b, c), probe in samples:
            sim = tip_palm_local({b1: a, b2: b, b3: c}, bone)
            err = np.linalg.norm(sim - np.array(probe))
            print('  %s angles=(%.1f,%.1f,%.1f): sim=%s probe=%s err=%.4f'
                  % (bone, a, b, c, np.round(sim, 3), np.round(probe, 3), err))

    print('\n=== 2. chain geometry (palm frame) and per-joint y lever ===')
    for finger in ('Middle', 'Ring'):
        b1, b2, b3 = BONES[finger]
        j1 = joint_pos[idx[b1]] - palm_pos
        j2 = joint_pos[idx[b2]] - palm_pos
        j3 = joint_pos[idx[b3]] - palm_pos
        s1 = j2 - j1
        s2 = j3 - j2
        s3 = TIP_OFFSET[b3]
        print('  %s: base=%s |s1|=%.3f |s2|=%.3f |s3|=%.3f  '
              'y-lever: M1=%.2f M2=%.2f M3=%.2f'
              % (finger, np.round(j1, 3), np.linalg.norm(s1), np.linalg.norm(s2),
                 np.linalg.norm(s3), np.linalg.norm(s1), np.linalg.norm(s2),
                 np.linalg.norm(s3)))
        # y of the tip for a given fold = base_y - sum of |s_i| sin(angles)
        print('     tip y(grip) = %.3f - %.3f*sin(A1*g) - %.3f*sin((A1+A2)*g)'
              ' - %.3f*sin((A1+A2+A3)*g)'
              % (j1[1] + s1[1] + s2[1] + s3[1], np.linalg.norm(s1),
                 np.linalg.norm(s2), np.linalg.norm(s3)))

    print('\n=== 3. palm flesh (vertices weighted to Palm) at finger z-bands ===')
    psel = [i for i in range(len(vc)) if int(v[2 * i]) == idx['Palm']]
    pv = pos[psel] - palm_pos
    for finger, z0 in (('Middle', 1.128), ('Ring', 1.091)):
        band = pv[(np.abs(pv[:, 2] - z0) < 0.6) & (pv[:, 0] >= -2.0) & (pv[:, 0] <= 3.0)]
        if len(band) == 0:
            print('  %s: no flesh verts in band' % finger)
            continue
        ys = np.sort(band[:, 1])
        print('  %s: %d verts  y min=%.3f p5=%.3f p25=%.3f med=%.3f max=%.3f'
              % (finger, len(band), ys[0], ys[len(ys) // 20], ys[len(ys) // 4],
                 ys[len(ys) // 2], ys[-1]))

    print('\n=== 4. current table (70/70/35) reference positions ===')
    for grip in (0.5, 0.6, 0.7, 1.0):
        for finger in ('Middle', 'Ring'):
            b3 = BONES[finger][2]
            t = tip_palm_local(fold(finger, grip, 70, 70, 35), b3)
            print('  grip=%.1f %-6s tip=%s' % (grip, finger, np.round(t, 3)))

    print('\n=== 5. search: minimal-deviation table with the tip on a natural band ===')
    print('palm inner face: y ~ -2.2 (Palm-weighted verts at the finger z-bands)')
    print('constraints: y(grip0.7) in [-4.0,-2.6] (near the palm zone, not 10 below)')
    print('             y(grip1.0) >= -5.2 (no deep plunge at full squeeze)')
    print('             natural fold shape: A1 >= A2 >= A3')
    print('cost = 2*|A1-70| + 2*|A2-70| + 1*|A3-35|  (M3 deviations half-weighted)')
    for finger in ('Middle', 'Ring'):
        b1, b2, b3 = BONES[finger]
        best = []
        for A1 in range(10, 41):
            for A2 in range(10, 41):
                if A2 > A1:
                    continue
                for A3 in range(0, min(36, A2 + 1)):
                    t07 = tip_palm_local(fold(finger, 0.7, A1, A2, A3), b3)
                    t10 = tip_palm_local(fold(finger, 1.0, A1, A2, A3), b3)
                    if not (-4.0 <= t07[1] <= -2.6):
                        continue
                    if t10[1] < -5.2:
                        continue
                    cost = 2 * abs(A1 - 70) + 2 * abs(A2 - 70) + abs(A3 - 35)
                    best.append((cost, A1, A2, A3))
        best.sort()
        print('\n  %s top-5 (cost | A1 A2 A3):' % finger)
        for cost, A1, A2, A3 in best[:5]:
            row = []
            for grip in (0.5, 0.6, 0.7, 1.0):
                t = tip_palm_local(fold(finger, grip, A1, A2, A3), b3)
                row.append('g%.1f=(%.2f,%.2f)' % (grip, t[0], t[1]))
            print('    cost=%3d | %2d %2d %2d | %s'
                  % (cost, A1, A2, A3, '  '.join(row)))
        if best:
            cost, A1, A2, A3 = best[0]
            dev = (A1 - 70, A2 - 70, A3 - 35)
            print('  -> best %s: (A1,A2,A3)=(%d,%d,%d)  deviation=%s (%+d%%,%+d%%,%+d%%)'
                  % (finger, A1, A2, A3, dev, 100 * dev[0] // 70,
                     100 * dev[1] // 70, 100 * dev[2] // 35))

    print('\n=== 6. proposed tables - full positions ===')
    for label, ang in (('A (M3-lean, recommended)', (17, 17, 10)),
                       ('B (min-deviation optimum)', (18, 17, 17))):
        print('\n  %s: Middle/Ring = (%d, %d, %d)' % ((label,) + ang))
        print('  %-6s %-10s %-10s %-10s' % ('grip', 'Middle', 'Ring', 'drop vs current'))
        for grip in (0.5, 0.6, 0.7, 1.0):
            tm = tip_palm_local(fold('Middle', grip, *ang), 'Middle3')
            tr = tip_palm_local(fold('Ring', grip, *ang), 'Ring3')
            cur = tip_palm_local(fold('Middle', grip, 70, 70, 35), 'Middle3')
            print('  %.1f    %s %s  %.1f -> %.1f'
                  % (grip, np.round(tm[:2], 3), np.round(tr[:2], 3),
                     cur[1], tm[1]))


if __name__ == '__main__':
    main()
