import sys
sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import numpy as np
import xml.etree.ElementTree as ET
import fix_rig_weights as fw

RAD = np.pi / 180.0
GRIP = {'Thumb1': 47, 'Thumb2': 40, 'Thumb3': 0}


def rmat(axis, deg):
    a = np.asarray(axis, dtype=float)
    a = a / np.linalg.norm(a)
    th = deg * RAD
    K = np.array([[0, -a[2], a[1]], [a[2], 0, -a[0]], [-a[1], a[0], 0]])
    return np.eye(3) + np.sin(th) * K + (1 - np.cos(th)) * (K @ K)


def load(path):
    root = ET.parse(path).getroot()
    src = {}
    for s in root.iter(fw.TNS + 'source'):
        f = s.find('c:float_array', fw.NS)
        if f is not None:
            src[s.get('id')] = np.array([float(x) for x in f.text.split()])
        na = s.find('c:Name_array', fw.NS)
        if na is not None:
            src[s.get('id')] = na.text.split()
    js_id = next(x for x in src if x.endswith('-joints'))
    joints = src[js_id]
    idx = {n: i for i, n in enumerate(joints)}
    jp = np.zeros((len(joints), 3))
    vs = root.find('.//c:visual_scene', fw.NS)
    def walk(node, p, acc):
        name = node.get('name')
        t = node.find('c:translate', fw.NS)
        loc = np.array([float(x) for x in t.text.split()]) if t is not None else np.zeros(3)
        a2 = loc if p is None else acc + loc
        if name in idx:
            jp[idx[name]] = a2
            p = idx[name]
        for ch in list(node):
            if ch.tag == fw.TNS + 'node':
                walk(ch, p, a2)
    walk(vs, None, np.zeros(3))
    return joints, jp


def thumb_fold(path, label, axes):
    joints, jp = load(path)
    idx = {n: i for i, n in enumerate(joints)}
    palm = jp[idx['Middle1']]
    base = jp[idx['Thumb1']]
    tip_bind = jp[idx['Thumb3']]
    print('%s: palm=%s thumb_base=%s thumb_tip=%s' % (label, np.round(palm, 2),
                                                      np.round(base, 2), np.round(tip_bind, 2)))
    for aname, ax in axes:
        R1 = rmat(ax, GRIP['Thumb1'])
        R2 = rmat(ax, GRIP['Thumb2'])
        Racc = {i: np.eye(3) for i in range(len(joints))}
        Racc[idx['Thumb1']] = R1
        Racc[idx['Thumb2']] = R1 @ R2
        Racc[idx['Thumb3']] = R1 @ R2
        r = tip_bind - base
        mdir = np.cross(np.asarray(ax) / np.linalg.norm(ax), r / np.linalg.norm(r))
        tip_end = base + Racc[idx['Thumb3']] @ r
        print('  %s: fold dir=%s tip_end=%s  dist_to_palm=%.2f'
              % (aname, np.round(mdir, 3), np.round(tip_end, 2),
                 np.linalg.norm(tip_end - palm)))


thumb_fold(r'E:\penumbra_vr-master\data\models\hud_objects\hud_object_hand_rig.dae', 'RIGHT',
           [('cur (0,+.891,+.454)', (0, 0.8910, 0.4540)),
            ('Y-flip (0,-.891,+.454)', (0, -0.8910, 0.4540)),
            ('neg (0,-.891,-.454)', (0, -0.8910, -0.4540)),
            ('mirror (0,+.891,-.454)', (0, 0.8910, -0.4540))])
thumb_fold(r'E:\penumbra_vr-master\data\models\hud_objects\hud_object_hand_left_rig.dae', 'LEFT',
           [('cur (0,+.891,-.454)', (0, 0.8910, -0.4540)),
            ('same-right (0,+.891,+.454)', (0, 0.8910, 0.4540)),
            ('neg (0,-.891,+.454)', (0, -0.8910, 0.4540)),
            ('Y-flip (0,-.891,-.454)', (0, -0.8910, -0.4540))])