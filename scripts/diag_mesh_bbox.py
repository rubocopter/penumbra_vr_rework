import sys
sys.path.insert(0, r'E:\penumbra_vr-master\scripts')
import numpy as np
import xml.etree.ElementTree as ET
import fix_rig_weights as fw

root = ET.parse(r'E:\penumbra_vr-master\data\models\hud_objects\hud_object_hand_rig.dae').getroot()
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
skin = root.find('.//c:controller/c:skin', fw.NS)
mesh = root.find('.//c:geometry/c:mesh', fw.NS)
tris = mesh.find('c:triangles', fw.NS)
p_in = tris.find("c:input[@semantic='POSITION']", fw.NS)
if p_in is None:
    vs = tris.find("c:input[@semantic='VERTEX']", fw.NS).get('source')[1:]
    p_in = mesh.find("c:vertices[@id='%s']/c:input[@semantic='POSITION']" % vs, fw.NS)
pos = src[p_in.get('source')[1:]].reshape(-1, 3)
vw = skin.find('c:vertex_weights', fw.NS)
vc = np.array([int(x) for x in vw.find('c:vcount', fw.NS).text.split()])
v = np.array([int(x) for x in vw.find('c:v', fw.NS).text.split()])
print('mesh bbox: x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]  n=%d' % (
    pos[:, 0].min(), pos[:, 0].max(), pos[:, 1].min(), pos[:, 1].max(),
    pos[:, 2].min(), pos[:, 2].max(), len(pos)))
for bn in ['Hand_Root', 'Palm', 'Middle1', 'Middle3', 'Thumb1', 'Thumb2', 'Thumb3', 'Little1']:
    bi = list(joints).index(bn)
    sel = np.array([i for i in range(len(vc)) if int(v[2 * i]) == bi])
    if len(sel) == 0:
        print('%s: no verts' % bn)
        continue
    b = pos[sel]
    print('%s: n=%d  x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]  centroid=%s' % (
        bn, len(sel), b[:, 0].min(), b[:, 0].max(), b[:, 1].min(), b[:, 1].max(),
        b[:, 2].min(), b[:, 2].max(), np.round(b.mean(axis=0), 2)))