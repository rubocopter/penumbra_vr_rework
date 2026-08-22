"""Verify the corrected per-hand fold directions and the Middle/Ring seam
behaviour before patching PlayerHands.cpp.

Checks:
1. Palmar-face side per hand: median Y of palm-face vertices near the finger
   bases (which side is 'into the palm').
2. Corrected left-hand axes (negated X/Z of the right-hand axes) fold the
   left fingertips TOWARD its palmar side (+Y), not away.
3. Middle/Ring seam: pairs of vertices from the two chains that lie close
   together at bind (<0.15 u) - how far they stretch apart at grip=1 under
   (a) the current diverging per-finger axes, (b) a shared +/-Z axis.

Analysis only - no game files are modified.
"""

import sys
import numpy as np

sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
from tune_finger_pose import (load, skin, verts_of, chain_points, FINGERS,
                              CHAINS)

PATHS = {
    'right': 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_rig.dae',
    'left': 'E:/penumbra_vr-master/data/models/hud_objects/hud_object_hand_left_rig.dae',
}

TABLE = {'Middle': (60, 70, 40), 'Ring': (60, 70, 40),
         'Little': (50, 60, 35), 'Index': (45, 55, 32),
         'Thumb': (75, 40)}


def table_to_deg(table):
    deg = {}
    for finger, angles in table.items():
        cum = 0.0
        for j, a in zip(CHAINS[finger], angles):
            cum += a
            deg[j] = cum
    return deg


def palmar_side(d):
    """Median Y of Palm-bone vertices near the finger-base region."""
    jp, idx, vc, v = d['jp'], d['idx'], d['vc'], d['v']
    palm_y = jp[idx['Palm']][1]
    sel = [k for k in range(len(vc)) if int(v[2 * k]) == idx['Palm']]
    ys = d['pos'][sel][:, 1] - palm_y
    # inner face = the side most of the palm flesh volume sits toward
    return float(np.median(ys))


def seam_stretch(d, deg, axes):
    """Bind-close Ring<->Middle vertex pairs: how far apart do they move."""
    out_r = skin(d, {}, axes)          # bind (no rotations)
    out_g = skin(d, deg, axes)         # grip
    iR, iM = verts_of(d, CHAINS['Ring']), verts_of(d, CHAINS['Middle'])
    pr, pm = d['pos'][iR], d['pos'][iM]
    dd = np.sqrt(((pr[:, None, :] - pm[None, :, :]) ** 2).sum(-1))
    ii, jj = np.where(dd < 0.15)
    if len(ii) == 0:
        return None
    gr, gm = out_g[iR], out_g[iM]
    stretched = np.sqrt(((gr[ii] - gm[jj]) ** 2).sum(-1))
    return len(ii), float(stretched.mean()), float(stretched.max())


def main():
    for hand, path in PATHS.items():
        d = load(path)
        sign = 1.0 if hand == 'right' else -1.0  # fold direction multiplier
        print('=== %s hand ===' % hand)
        print('  palmera (mediana Y de carne palmar rel palm): %+0.2f'
              % palmar_side(d))

        # current diverging per-finger axes
        cur_axes = {
            'Middle': np.array([-0.221, 0, -0.975]),
            'Ring': np.array([0.168, 0, -0.986]),
            'Little': np.array([sign * -0.01, 0, sign * -1.0]),
            'Index': np.array([sign * -0.033, 0, sign * -0.999]),
            'Thumb': np.array([0, 0.985, -0.174 * sign]),
        }
        # unified +/-Z finger axes (per-hand sign), thumb mirrored
        fix_axes = {
            'Middle': np.array([0, 0, -sign]),
            'Ring': np.array([0, 0, -sign]),
            'Little': np.array([0, 0, -sign]),
            'Index': np.array([0, 0, -sign]),
            'Thumb': np.array([0, 0.985, -0.174 * sign]),
        }
        deg = table_to_deg(TABLE)

        for label, axes in (('current-diverging', cur_axes),
                            ('fixed-unified', fix_axes)):
            r = seam_stretch(d, deg, axes)
            if r is None:
                print('  [%s] seam pairs: none <0.15' % label)
                continue
            n, mean, mx = r
            print('  [%-17s] seam pairs=%4d  stretch mean=%.2f max=%.2f'
                  % (label, n, mean, mx))

            # tip travel of Middle and Little: confirm fold direction
            jbind = chain_points(d, 'Middle', {}, axes)[-1]
            jgrip = chain_points(d, 'Middle', deg, axes)[-1]
            dtip = jgrip - jbind
            lbind = chain_points(d, 'Little', {}, axes)[-1]
            lgrip = chain_points(d, 'Little', deg, axes)[-1]
            ltip = lgrip - lbind
            t_bind = chain_points(d, 'Thumb', {}, axes)[-1]
            t_grip = chain_points(d, 'Thumb',
                                  table_to_deg({'Thumb': TABLE['Thumb']}), axes)[-1]
            ttip = t_grip - t_bind
            print('      dTip Middle=%s Little=%s Thumb=%s'
                  % (np.round(dtip, 2), np.round(ltip, 2), np.round(ttip, 2)))


if __name__ == '__main__':
    main()
