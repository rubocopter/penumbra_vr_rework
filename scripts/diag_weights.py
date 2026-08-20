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
vw = skin.find('c:vertex_weights', fw.NS)
vc = np.array([int(x) for x in vw.find('c:vcount', fw.NS).text.split()])
v = np.array([int(x) for x in vw.find('c:v', fw.NS).text.split()])
wid = next(inp.get('source')[1:] for inp in vw.findall(fw.TNS + 'input')
           if inp.get('semantic') == 'WEIGHT')
weights = src[wid]
print('weights array: %d values' % len(weights))
print('unique weight values:', np.unique(np.round(weights, 6)))
per = {}
for i in range(len(vc)):
    j = int(v[2 * i])
    w = float(weights[int(v[2 * i + 1])])
    per.setdefault(joints[j], []).append(w)
for j in joints:
    ws = per.get(j, [])
    if ws:
        a = np.array(ws)
        print('%-10s n=%4d  min=%.4f max=%.4f  mean=%.4f' % (j, len(a), a.min(), a.max(), a.mean()))
    else:
        print('%-10s NO VERTICES' % j)