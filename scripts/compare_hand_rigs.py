"""Compare the left and right rigged hand DAE files: joint positions, chain
directions and derived anatomical vertical axes. Decides whether the left rig
is a mirrored copy or independent geometry, and prints the per-hand axes that
PlayerHands.cpp should use.

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

CHAINS = {
    'Middle': ('Middle1', 'Middle2', 'Middle3'),
    'Ring': ('Ring1', 'Ring2', 'Ring3'),
    'Little': ('Little1', 'Little2', 'Little3'),
    'Index': ('Index1', 'Index2', 'Index3'),
    'Thumb': ('Thumb1', 'Thumb2', 'Thumb3'),
}


def norm(v):
    return v / (np.linalg.norm(v) + 1e-12)


def vertical_axis(d):
    """Anatomical vertical fold axis: finger curls downward (-Y) inside its
    own sagittal plane."""
    av = norm(np.cross(norm(d), np.array([0.0, -1.0, 0.0])))
    return av


def main():
    data = {}
    for hand, path in PATHS.items():
        tree, root, pos, joints, joint_pos, local_trans, vc, v, ibm = dg.parse(path)
        idx = {n: i for i, n in enumerate(joints)}
        data[hand] = dict(pos=pos, joints=joints, jp=joint_pos, lt=local_trans,
                          vc=vc, v=v, idx=idx)

        print('=== %s hand ===' % hand)
        palm = joint_pos[idx['Palm']]
        print('  Palm=%s' % np.round(palm, 3))
        for finger, bones in CHAINS.items():
            j = [joint_pos[idx[b]] - palm for b in bones]
            s1, s2 = j[1] - j[0], j[2] - j[1]
            d = norm(s1)
            av = vertical_axis(np.linalg.norm(s1) * d)
            # sign: rotating around +av must move the tip DOWN (-Y)
            r_tip = j[0] + s1 + s2
            if np.cross(av, r_tip - j[0])[1] > 0:
                av = -av
            print('  %-6s base=%s segs=%.2f/%.2f dir=%s vert_axis=%s'
                  % (finger, np.round(j[0], 2),
                     np.linalg.norm(s1), np.linalg.norm(s2),
                     np.round(d, 3), np.round(av, 3)))
        print()

    # Are the two rigs the same geometry?
    jr, jl = data['right']['jp'], data['left']['jp']
    same_names = data['right']['joints'] == data['left']['joints']
    if same_names:
        diffs = np.abs(jr - jl)
        print('=== left vs right joint positions ===')
        print('  max|diff|=%.4f  mean|diff|=%.4f' % (diffs.max(), diffs.mean()))
        worst = int(np.argmax(diffs.max(axis=1)))
        print('  largest: %s (%s)' % (data['right']['joints'][worst],
                                      np.round(diffs[worst], 3)))
        mirrored_x = np.abs(jr[:, 0] + jl[:, 0]).max()
        print('  mirror-X test: max|x_r + x_l|=%.4f' % mirrored_x)


if __name__ == '__main__':
    main()
