"""Re-tune the finger fold axes and angles from bind geometry.

Derives, per hand rig:
- Anatomical vertical fold axes for Middle/Ring/Little/Index (curl straight
  down each finger's own sagittal plane -> no lateral twisting).
- A thumb fold axis aimed at the index-knuckle zone (visible opposition),
  found by searching the axis that steers the thumb tip toward that zone.

Then simulates candidate angle tables with the engine-exact skinning
(fixed pivots, single-weight tubes) and reports per-finger tip trajectories,
adjacent-finger clearances and palm-face penetration, rendering bind vs
grip=1 comparisons to scripts/tune_*.png for visual review.

Analysis only - no game files are modified.
"""

import sys
import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import diag_rig_geometry as dg

PATHS = {
    'right': 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae',
    'left': 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae',
}

RAD = np.pi / 180.0
CHAINS = {
    'Middle': ('Middle1', 'Middle2', 'Middle3'),
    'Ring': ('Ring1', 'Ring2', 'Ring3'),
    'Little': ('Little1', 'Little2', 'Little3'),
    'Index': ('Index1', 'Index2', 'Index3'),
    'Thumb': ('Thumb1', 'Thumb2', 'Thumb3'),
}
FINGERS = ['Little', 'Ring', 'Middle', 'Index']  # z order on the knuckle line


def norm(v):
    return v / (np.linalg.norm(v) + 1e-12)


def rmat(axis, deg):
    a = np.asarray(axis, dtype=float)
    a = a / np.linalg.norm(a)
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def load(path):
    tree, root, pos, joints, joint_pos, local_trans, vc, v, ibm = dg.parse(path)
    idx = {n: i for i, n in enumerate(joints)}
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

    # triangle indices for rendering
    tri = root.find('.//c:mesh/c:triangles', dg.fw.NS)
    ins = tri.findall('c:input', dg.fw.NS)
    stride = max(int(i.get('offset')) for i in ins) + 1
    p = [int(x) for x in tri.find('c:p', dg.fw.NS).text.split()]
    tris = [tuple(p[i:i + stride * 3:stride]) for i in range(0, len(p) - stride * 2, stride)]

    return dict(pos=pos, joints=joints, jp=joint_pos, lt=local_trans, vc=vc,
                v=v, idx=idx, parent=parent, tris=tris)


def chain_points(d, finger, deg_by_joint, axes):
    """World positions of the three joints of one chain under a pose."""
    ws = {}
    out = []

    def world(name):
        if name in ws:
            return ws[name]
        i = d['idx'][name]
        par = d['parent'][i]
        R = np.eye(3)
        ch = name.rstrip('0123')
        deg = deg_by_joint.get(name, 0.0)
        if deg and ch in axes:
            R = rmat(axes[ch], deg)
        L = np.eye(4)
        L[:3, :3] = R
        L[:3, 3] = d['lt'][i]
        W = L if par is None else world(d['joints'][par]) @ L
        ws[name] = W
        return W

    for b in CHAINS[finger]:
        out.append(world(b)[:3, 3].copy())
    return out


def skin(d, deg_by_joint, axes):
    """Engine-exact skinned vertex positions."""
    n = len(d['joints'])
    ws = []
    for i in range(n):
        name = d['joints'][i]
        deg = deg_by_joint.get(name, 0.0)
        R = np.eye(3)
        if deg:
            ax = axes[name.rstrip('0123')]
            R = rmat(ax, deg)
        L = np.eye(4)
        L[:3, :3] = R
        L[:3, 3] = d['lt'][i]
        par = d['parent'][i]
        ws.append(L if par is None else ws[par] @ L)
    out = np.zeros_like(d['pos'])
    jp = d['jp']
    for i, p in enumerate(d['pos']):
        j = int(d['v'][2 * i])
        out[i] = (ws[j] @ np.append(p - jp[j], 1.0))[:3]
    return out


def verts_of(d, bones):
    bs = {d['idx'][b] for b in bones}
    all_bones = set()
    for b in bones:
        i = d['idx'][b]
        all_bones.add(i)
    sel = [k for k in range(len(d['vc'])) if int(d['v'][2 * k]) in bs]
    return np.array(sel, dtype=int)


def derive_axes(d):
    """Per-hand fold axes: anatomical vertical for the four fingers, thumb
    axis searched so the folded tip approaches the index-knuckle zone."""
    jp, idx = d['jp'], d['idx']
    palm = jp[idx['Palm']]
    axes = {}
    info = {}

    for finger in FINGERS:
        b1, b2, b3 = CHAINS[finger]
        j1 = jp[idx[b1]]
        j2 = jp[idx[b2]]
        j3 = jp[idx[b3]]
        dseg = norm(j2 - j1)
        av = norm(np.cross(dseg, np.array([0.0, -1.0, 0.0])))
        if np.cross(av, j3 - j1)[1] > 0:
            av = -av
        axes[finger] = av
        info[finger] = dict(dir=dseg, base=j1 - palm)

    # thumb: search fold axes tilted in the YZ plane so the folded tip both
    # crosses toward the fingers (-Z) and descends (-Y). Score = distance of
    # the fully folded tip to an aim point below the index knuckle.
    tb1, tb2, tb3 = CHAINS['Thumb']
    t1 = jp[idx[tb1]]
    dchain = norm(jp[idx[tb2]] - t1)
    seg2 = jp[idx[tb3]] - jp[idx[tb2]]
    index_base = jp[idx['Index1']]
    aim = index_base + np.array([0.0, -1.2, 0.0])

    def thumb_tip(axis, a1, a2):
        R1 = rmat(axis, a1)[:3, :3]
        j2 = t1 + R1 @ (jp[idx[tb2]] - t1)
        d2 = norm(j2 - t1)
        R12 = rmat(axis, a1 + a2)[:3, :3]
        return j2 + R12 @ seg2

    best_ax, best_d, best_cfg = None, None, None
    scored = []
    for phi in range(-80, 81, 10):
        ax = np.array([0.0, np.cos(np.radians(phi)), np.sin(np.radians(phi))])
        if abs(np.dot(ax, dchain)) > 0.98:
            continue
        for sgn in (1.0, -1.0):
            ax_s = ax * sgn
            for a1, a2 in ((35, 20), (45, 25), (55, 30), (65, 35), (75, 40)):
                tip = thumb_tip(ax_s, a1, a2)
                dd = np.linalg.norm(tip - aim)
                scored.append((dd, phi, sgn, (a1, a2), tip.copy()))
    scored.sort(key=lambda r: r[0])
    print('  top-6 thumb axes (aim_dist | phi sgn angles | dTip rel bind):')
    t_bind = t1 + (jp[idx[tb2]] - t1) + seg2
    for dd, phi, sgn, ang, tip in scored[:6]:
        ax_s = np.array([0.0, np.cos(np.radians(phi)), np.sin(np.radians(phi))]) * sgn
        dt = tip - t_bind
        print('    %.2f | phi=%+3d sgn=%+d %s axis=%s dTip=%s'
              % (dd, phi, int(sgn), ang, np.round(ax_s, 3), np.round(dt, 2)))
    best_d, _, _, best_cfg, best_tip = scored[0]
    best_ax = np.array([0.0, np.cos(np.radians(scored[0][1])),
                        np.sin(np.radians(scored[0][1]))]) * scored[0][2]
    axes['Thumb'] = best_ax
    info['Thumb'] = dict(dir=dchain, base=t1 - palm,
                         reach=np.linalg.norm(jp[idx[tb3]] - t1),
                         aim_dist=best_d, angles=best_cfg,
                         tip=best_tip - palm)
    return axes, info


def table_to_deg(table):
    deg = {}
    for finger, angles in table.items():
        cum = 0.0
        for j, a in zip(CHAINS[finger], angles):
            cum += a
            deg[j] = cum
    return deg


def evaluate(hand, d, axes, table, label, verbose=True):
    deg = table_to_deg(table)
    out = skin(d, deg, axes)
    jp, idx, vc, v = d['jp'], d['idx'], d['vc'], d['v']
    palm = jp[idx['Palm']]

    clouds = {}
    rows = []
    for finger in FINGERS + ['Thumb']:
        bones = CHAINS[finger]
        sel = verts_of(d, bones)
        pts = out[sel]
        clouds[finger] = pts
        tip = chain_points(d, finger, deg, axes)[-1]
        rows.append('%-6s tip rel palm=%s' % (finger, np.round(tip - palm, 2)))

    # clearance between adjacent finger vertex clouds (bind baseline first)
    adj = [('Little', 'Ring'), ('Ring', 'Middle'), ('Middle', 'Index'),
           ('Index', 'Thumb')]
    clears = []
    for a, b in adj:
        pa = clouds[a] if label != 'bind' else d['pos'][verts_of(d, CHAINS[a])]
        pb = clouds[b] if label != 'bind' else d['pos'][verts_of(d, CHAINS[b])]
        dd = np.sqrt(((pa[:, None, :] - pb[None, :, :]) ** 2).sum(-1))
        clears.append('%s-%s=%.2f' % (a[0], b[0], dd.min()))
    if verbose:
        print('  [%s/%s] %s | clear: %s'
              % (hand, label, '  '.join(rows), ' '.join(clears)))
    return out


CANDIDATES = {
    #          Middle        Ring          Little        Index(trig)   Thumb
    'light': {'Middle': (40, 45, 25), 'Ring': (40, 45, 25),
              'Little': (34, 40, 22), 'Index': (30, 38, 20), 'Thumb': (40, 25)},
    'medium': {'Middle': (50, 58, 32), 'Ring': (50, 58, 32),
               'Little': (42, 50, 28), 'Index': (38, 46, 26), 'Thumb': (55, 35)},
    'strong': {'Middle': (60, 70, 40), 'Ring': (60, 70, 40),
               'Little': (50, 60, 35), 'Index': (45, 55, 32), 'Thumb': (70, 45)},
}


def render(d, axes, table, path_out):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    from matplotlib.collections import PolyCollection

    deg = table_to_deg(table)
    out = skin(d, deg, axes)
    tris = d['tris']

    views = [('front (-X)', lambda p: (p[2], -p[1])),
             ('side (-Z)', lambda p: (p[0], -p[1])),
             ('top (+Y)', lambda p: (p[0], -p[2]))]
    fig, axs = plt.subplots(len(views), 2, figsize=(13, 6.2 * len(views)))
    for vi, (vname, proj) in enumerate(views):
        for ai, (nm, P) in enumerate([('bind', d['pos']), ('grip=1', out)]):
            ax = axs[vi][ai]
            ax.set_title('%s | %s' % (vname, nm), fontsize=10)
            polys = [[proj(P[i]) for i in t] for t in tris]
            pc = PolyCollection(polys, facecolors='#b08050', edgecolors='none')
            ax.add_collection(pc)
            # draw joints of the chains
            for finger, col in (('Thumb', 'tab:red'), ('Index', 'tab:blue'),
                                ('Middle', 'tab:green'), ('Ring', 'tab:orange'),
                                ('Little', 'tab:purple')):
                pts = chain_points(d, finger, deg, axes)
                xy = np.array([proj(p) for p in pts])
                ax.plot(xy[:, 0], xy[:, 1], '.', color=col, markersize=4)
            ax.autoscale()
            ax.set_aspect('equal')
    fig.tight_layout()
    fig.savefig(path_out, dpi=100)
    plt.close(fig)


def main():
    for hand, path in PATHS.items():
        d = load(path)
        axes, info = derive_axes(d)
        print('=== %s hand ===' % hand)
        print('  derived axes:')
        for k in FINGERS + ['Thumb']:
            if k == 'Thumb':
                print('    %-6s axis=%s aim_dist=%.2f angles=%s tip=%s'
                      % (k, np.round(axes[k], 3), info['Thumb']['aim_dist'],
                         info['Thumb']['angles'], np.round(info['Thumb']['tip'], 2)))
            else:
                print('    %-6s axis=%s' % (k, np.round(axes[k], 3)))
        evaluate(hand, d, axes, {}, 'bind')
        for name, table in CANDIDATES.items():
            evaluate(hand, d, axes, table, name)
        # render medium + strong for visual inspection
        render(d, axes, CANDIDATES['medium'],
               r'E:\penumbra_vr-master\scripts\tune_%s_medium.png' % hand)
        render(d, axes, CANDIDATES['strong'],
               r'E:\penumbra_vr-master\scripts\tune_%s_strong.png' % hand)
    print('renders written')


if __name__ == '__main__':
    main()
