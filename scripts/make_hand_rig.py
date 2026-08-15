#!/usr/bin/env python3
"""
make_hand_rig.py - Generates rigged (skinned) hand models for Penumbra VR.

Reads the existing static hud_object_hand.dae / hud_object_hand_left.dae meshes,
computes a simple finger skeleton from the geometry, assigns vertex weights
(nearest joint, 1 influence per vertex) and emits a new .dae with:

  * library_controllers (skin: bind_shape identity, joints, inv_bind matrices,
    vertex_weights) - exactly matching what HPL1's Collada loader expects
    (MeshLoaderCollada.cpp / MeshLoaderColladaHelpers.cpp).
  * visual_scene with a JOINT node hierarchy (bone names = node ids) + an
    instance_controller node.

The animations (Grab / Trigger / Index) are NOT stored in the file; they are
built in code (PlayerHands.cpp) with per-bone keyframes, since HPL1 merges all
.anm tracks of a mesh into a single "Default" animation.

Anatomy notes (validated against the actual meshes):
  * The whole mesh is ONE connected component: an open fanned hand (fingers
    along +X, spread across Z, palm facing +Y for the right hand) plus a large
    hollow coat-sleeve (C-tube, x in [-18, ~8.5]) attached at the wrist.
  * The ring and middle fingers are a webbed double-tube: four wall clusters
    at the base (x in [8.5,10]) that fuse into one component from x ~ 9.15 to
    the shared tip at x ~ 21.2. The grip animation curls ring and middle with
    identical angles, so the web is never torn. The other fingers are clean
    C-tubes: little to 18.95, index to 18.56, thumb (solid thenar) to 12.88.
  * Everything with x < 9.0 is rigid (sleeve, sleeve mouth walls reaching
    x ~ 8.5 with z > -3.5, wrist tube, thenar front) and is forced to
    Hand_Root via a weight override. Cross-section band tests are fragile
    here (a thin cone-tip sliver inside the sleeve dominates them).
  * Flexion axis = +-Z (finger spread axis); the sign comes from a curl test
    that rotates each fingertip toward the palm front (palm_center offset by
    the dominant Y-normal sign of the palm surface).

Usage: python scripts/make_hand_rig.py
"""

import os
import re
import math
import xml.etree.ElementTree as ET

NS = "http://www.collada.org/2005/11/COLLADASchema"

DATA_DIR = os.path.join(os.path.dirname(__file__), "..", "data", "models", "hud_objects")

RIGHT_SRC = os.path.join(DATA_DIR, "hud_object_hand.dae")
LEFT_SRC = os.path.join(DATA_DIR, "hud_object_hand_left.dae")
RIGHT_OUT = os.path.join(DATA_DIR, "hud_object_hand_rig.dae")
LEFT_OUT = os.path.join(DATA_DIR, "hud_object_hand_left_rig.dae")

FINGER_NAMES = ["Little", "Ring", "Middle", "Index", "Thumb"]  # by z-rank of the base

# ---------------------------------------------------------------------------
# Small vector helpers (stdlib only)
# ---------------------------------------------------------------------------

def vsub(a, b): return (a[0]-b[0], a[1]-b[1], a[2]-b[2])
def vadd(a, b): return (a[0]+b[0], a[1]+b[1], a[2]+b[2])
def vscale(a, s): return (a[0]*s, a[1]*s, a[2]*s)
def vdot(a, b): return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]
def vnorm(a):
    l = math.sqrt(vdot(a, a))
    return (a[0]/l, a[1]/l, a[2]/l) if l > 1e-9 else (0.0, 0.0, 0.0)
def vlen(a): return math.sqrt(vdot(a, a))
def vlerp(a, b, t): return vadd(a, vscale(vsub(b, a), t))

def rot_axis(v, axis, ang_deg):
    """Rotate vector v around unit axis by ang_deg (right-hand rule)."""
    a = math.radians(ang_deg)
    c, s = math.cos(a), math.sin(a)
    k = axis
    return (
        v[0]*(c + (1-c)*k[0]*k[0]) + v[1]*((1-c)*k[0]*k[1] - s*k[2]) + v[2]*((1-c)*k[0]*k[2] + s*k[1]),
        v[0]*((1-c)*k[1]*k[0] + s*k[2]) + v[1]*(c + (1-c)*k[1]*k[1]) + v[2]*((1-c)*k[1]*k[2] - s*k[0]),
        v[0]*((1-c)*k[2]*k[0] - s*k[1]) + v[1]*((1-c)*k[2]*k[1] + s*k[0]) + v[2]*(c + (1-c)*k[2]*k[2]),
    )

# ---------------------------------------------------------------------------
# XML helpers
# ---------------------------------------------------------------------------

def q(tag):
    return "{%s}%s" % (NS, tag)

def _wrap(path):
    return "/".join(q(p) for p in path.split("/"))

def find_local(elem, path):
    return elem.find(_wrap(path))

def findall_local(elem, path):
    return elem.findall(_wrap(path))

def parse_float_array(src_text):
    return [float(x) for x in src_text.split()]

# ---------------------------------------------------------------------------
# Geometry extraction
# ---------------------------------------------------------------------------

def extract_geometry(path):
    """Returns (positions, tri_position_indices, geom_id, material_symbol, normals)."""
    root = ET.parse(path).getroot()
    geom = None
    for g in findall_local(root, "library_geometries/geometry"):
        if find_local(g, "mesh") is not None:
            geom = g
            break
    if geom is None:
        raise RuntimeError("no geometry with <mesh> found in %s" % path)

    geom_id = geom.get("id")
    mesh = find_local(geom, "mesh")

    positions = []
    normals = []
    tri_idx = []
    material = None
    vertices_id = None

    for src in findall_local(mesh, "source"):
        sid = src.get("id")
        fa = find_local(src, "float_array")
        acc = find_local(find_local(src, "technique_common"), "accessor")
        if fa is None or acc is None:
            continue
        stride = int(acc.get("stride", "1"))
        vals = parse_float_array(fa.text)
        name = sid.split("-")[-1].lower()
        if name.startswith("position") and not positions:
            positions = [tuple(vals[i:i+3]) for i in range(0, len(vals), stride)]
        elif name.startswith("normal") and not normals:
            normals = [tuple(vals[i:i+3]) for i in range(0, len(vals), stride)]

    verts = find_local(mesh, "vertices")
    if verts is not None:
        vertices_id = verts.get("id")

    tri = find_local(mesh, "triangles")
    if tri is not None:
        material = tri.get("material")
        inputs = findall_local(tri, "input")
        off_map = {}
        for inp in inputs:
            off_map[inp.get("semantic")] = int(inp.get("offset", "0"))
        p = [int(x) for x in find_local(tri, "p").text.split()]
        stride = len(inputs)
        voff = off_map["VERTEX"]
        tri_idx = [p[i*stride + voff] for i in range(len(p) // stride)]

    return positions, tri_idx, geom_id, material, normals

# ---------------------------------------------------------------------------
# Deterministic k-means (farthest-first seeding)
# ---------------------------------------------------------------------------

def kmeans(points, k, iters=80):
    """Returns (centers, clusters): clusters[j] = list of indices into points."""
    m = len(points)
    if m <= k:
        return list(points), [[i] for i in range(m)]
    centers = [points[0]]
    while len(centers) < k:
        far = max(range(m), key=lambda i: min(vdot(vsub(points[i], c), vsub(points[i], c)) for c in centers))
        if vlen(vsub(points[far], centers[-1])) < 1e-12:
            break
        centers.append(points[far])
    centers = centers[:k]
    clusters = [[] for _ in range(len(centers))]
    for _ in range(iters):
        clusters = [[] for _ in range(len(centers))]
        for i in range(m):
            best = min(range(len(centers)), key=lambda j: vdot(vsub(points[i], centers[j]), vsub(points[i], centers[j])))
            clusters[best].append(i)
        moved = False
        for j in range(len(centers)):
            if not clusters[j]:
                continue
            c = clusters[j]
            n = len(c)
            newc = (sum(points[i][0] for i in c)/n, sum(points[i][1] for i in c)/n, sum(points[i][2] for i in c)/n)
            if vlen(vsub(newc, centers[j])) > 1e-6:
                moved = True
            centers[j] = newc
        if not moved:
            break
    return centers, clusters

# ---------------------------------------------------------------------------
# Mesh adjacency / tube tracing
# ---------------------------------------------------------------------------

def build_adjacency(n, tri_idx):
    adj = [set() for _ in range(n)]
    for i in range(0, len(tri_idx), 3):
        a, b, c = tri_idx[i], tri_idx[i+1], tri_idx[i+2]
        adj[a].add(b); adj[b].add(a)
        adj[b].add(c); adj[c].add(b)
        adj[c].add(a); adj[a].add(c)
    return adj

def trace_tube(adj, positions, seeds, x_min):
    """BFS from seeds, only expanding to vertices with x >= x_min."""
    visited = set(seeds)
    stack = list(seeds)
    while stack:
        i = stack.pop()
        for j in adj[i]:
            if j not in visited and positions[j][0] >= x_min:
                visited.add(j)
                stack.append(j)
    return visited

def centroid_of(pts):
    n = len(pts)
    return (sum(p[0] for p in pts)/n, sum(p[1] for p in pts)/n, sum(p[2] for p in pts)/n)

# ---------------------------------------------------------------------------
# Skeleton computation
# ---------------------------------------------------------------------------

def compute_skeleton(positions, tri_idx, normals):
    """Returns dict with bones (name, parent, global pos), palm info, axes."""

    n = len(positions)
    bbox_min = [min(p[i] for p in positions) for i in range(3)]
    bbox_max = [max(p[i] for p in positions) for i in range(3)]

    # PCA of the vertex cloud -> dominant axis (report only; must be ~ +X)
    centroid = centroid_of(positions)
    cov = [[0.0]*3 for _ in range(3)]
    for p in positions:
        d = vsub(p, centroid)
        for i in range(3):
            for j in range(3):
                cov[i][j] += d[i]*d[j]
    for i in range(3):
        for j in range(3):
            cov[i][j] /= n
    v = (1.0, 0.0, 0.0)
    for _ in range(50):
        nv = (cov[0][0]*v[0]+cov[0][1]*v[1]+cov[0][2]*v[2],
              cov[1][0]*v[0]+cov[1][1]*v[1]+cov[1][2]*v[2],
              cov[2][0]*v[0]+cov[2][1]*v[1]+cov[2][2]*v[2])
        v = vnorm(nv)
    pca_axis = v

    # ------------------------------------------------------------------
    # Structural hand boundary: everything with x < HAND_MIN_X is rigid and
    # forced to Hand_Root. This covers the whole sleeve (x in [-18, ~8.5]),
    # the sleeve mouth walls, the wrist tube and the thenar front (the
    # fingers split from the palm between x=8.5 and 10, and the sleeve
    # mouth ends at x ~ 8.5). Cross-section band tests are fragile here:
    # the sleeve interior contains a thin cone-tip sliver that dominates
    # the "narrowest cross-section" metric.
    # ------------------------------------------------------------------
    HAND_MIN_X = 9.0

    # Palm center: centroid of the palm band (x in [9,13], z in [-9,6]).
    palm_band = [p for p in positions if 9.0 <= p[0] <= 13.0 and -9.0 <= p[2] <= 6.0]
    palm_center = centroid_of(palm_band) if palm_band else centroid

    # Wrist point: centroid of the wrist tube just behind the boundary.
    wrist_band = [p for p in positions if 6.0 <= p[0] < HAND_MIN_X and -9.0 <= p[2] <= -3.5]
    wrist = centroid_of(wrist_band) if wrist_band else vadd(palm_center, vscale(pca_axis, -2.0))

    # ------------------------------------------------------------------
    # Finger bases: k-means (k=10) on the split band x in [8.5, 10].
    # Each finger tube (2 walls) yields 2 clusters; the thenar yields the
    # rest. Sort clusters by z, pair consecutive: Little..Thumb by z-rank.
    # ------------------------------------------------------------------
    base_band_idx = [i for i, p in enumerate(positions) if 8.5 <= p[0] <= 10.0]
    base_pts = [positions[i] for i in base_band_idx]
    centers, clusters = kmeans(base_pts, 10)
    order = sorted(range(len(centers)), key=lambda k: centers[k][2])
    pairs = [(order[2*i], order[2*i+1]) for i in range(min(5, len(order)//2))]

    adj = build_adjacency(len(positions), tri_idx)
    fingers = []
    for name, (ca, cb) in zip(FINGER_NAMES, pairs):
        seed_idx = [base_band_idx[j] for j in clusters[ca] + clusters[cb]]
        tube = trace_tube(adj, positions, seed_idx, 9.0)
        tip = max((positions[i] for i in tube), key=lambda p: p[0])
        base = centroid_of([positions[i] for i in seed_idx])
        fingers.append((name, base, tip))

    # ------------------------------------------------------------------
    # Flexion axis: the finger spread direction (fingers curl around it
    # toward the palm), snapped to the dominant world axis (+-Z here).
    # ------------------------------------------------------------------
    spread = vnorm(vsub(fingers[4][1], fingers[0][1])) if len(fingers) >= 5 else (0.0, 0.0, 1.0)
    world_axes = [(1,0,0), (-1,0,0), (0,1,0), (0,-1,0), (0,0,1), (0,0,-1)]
    flex_base = max(world_axes, key=lambda a: abs(vdot(a, spread)))

    # Palm front: sign of the dominant Y normal on the palm surface.
    if normals:
        pos_cnt = sum(1 for nn in normals if nn[1] > 0.5)
        neg_cnt = sum(1 for nn in normals if nn[1] < -0.5)
        front = 1.0 if pos_cnt >= neg_cnt else -1.0
    else:
        front = 1.0
    palm_front = (palm_center[0], palm_center[1] + front * 1.0, palm_center[2])

    # Sign: pick the rotation direction that moves the fingertips toward
    # the palm front (curl test, 60 deg around the base).
    scores = {}
    for sign in (1.0, -1.0):
        axis = tuple(sign * c for c in flex_base)
        tot = 0.0
        for name, base, tip in fingers:
            tip_rel = vsub(tip, base)
            new_tip = vadd(base, rot_axis(tip_rel, axis, 60.0))
            d_before = vlen(vsub(tip, palm_front))
            d_after = vlen(vsub(new_tip, palm_front))
            tot += d_before - d_after
        scores[sign] = tot
    flex_sign = 1.0 if scores[1.0] >= scores[-1.0] else -1.0
    flex_axis = tuple(flex_sign * c for c in flex_base)

    # ------------------------------------------------------------------
    # Bone hierarchy (global positions, identity rotations).
    # ------------------------------------------------------------------
    bones = []
    def add_bone(name, parent, pos):
        bones.append({"name": name, "parent": parent, "pos": pos})

    add_bone("Hand_Root", None, wrist)
    add_bone("Palm", "Hand_Root", palm_center)

    for name, base, tip in fingers:
        add_bone(name + "1", "Palm", base)
        add_bone(name + "2", name + "1", vlerp(base, tip, 0.55))
        add_bone(name + "3", name + "2", vlerp(base, tip, 0.92))

    return {
        "bones": bones,
        "hand_min_x": HAND_MIN_X,
        "pca_axis": pca_axis,
        "spread_axis": spread,
        "flexion_axis": flex_axis,
        "flexion_sign": flex_sign,
        "palm_front": palm_front,
        "bbox": (bbox_min, bbox_max),
        "palm_center": palm_center,
        "wrist": wrist,
        "fingers": fingers,
    }

# ---------------------------------------------------------------------------
# Weight assignment
# ---------------------------------------------------------------------------

def assign_weights(positions, bones, hand_min_x):
    """Nearest joint per vertex, 1 influence, weight 1.0.
    Override: everything with x < hand_min_x (sleeve, sleeve mouth walls,
    wrist tube, thenar front) -> Hand_Root."""
    joint_pos = [b["pos"] for b in bones]
    pairs = []
    for p in positions:
        if p[0] < hand_min_x:
            pairs.append(0)  # Hand_Root
        else:
            pairs.append(min(range(len(joint_pos)), key=lambda i: vlen(vsub(p, joint_pos[i]))))
    return pairs

# ---------------------------------------------------------------------------
# XML generation (string based, matches loader expectations)
# ---------------------------------------------------------------------------

def fmt(v):
    return "%.6f" % v

def make_controller_xml(geom_id, bones, pair_joint_idx):
    n = len(bones)
    joint_names = " ".join(b["name"] for b in bones)

    inv_bind = []
    for b in bones:
        g = b["pos"]
        inv_bind.append("1 0 0 0 0 1 0 0 0 0 1 0 %.6f %.6f %.6f 1" % (-g[0], -g[1], -g[2]))

    vcount = " ".join(["1"] * len(pair_joint_idx))
    v = " ".join("%d 0" % j for j in pair_joint_idx)

    return (
        '  <library_controllers>\n'
        '    <controller id="Hand_Ctrl" name="Hand_Ctrl">\n'
        '      <skin source="#%s">\n'
        '        <bind_shape_matrix>1 0 0 0 0 1 0 0 0 0 1 0 0 0 0 1</bind_shape_matrix>\n'
        '        <source id="Hand_Ctrl-joints">\n'
        '          <Name_array id="Hand_Ctrl-joints-array" count="%d">%s</Name_array>\n'
        '          <technique_common><accessor source="#Hand_Ctrl-joints-array" count="%d" stride="1"><param name="JOINT" type="Name"/></accessor></technique_common>\n'
        '        </source>\n'
        '        <source id="Hand_Ctrl-bind_poses">\n'
        '          <float_array id="Hand_Ctrl-bind_poses-array" count="%d">%s</float_array>\n'
        '          <technique_common><accessor source="#Hand_Ctrl-bind_poses-array" count="%d" stride="16"><param name="TRANSFORM" type="float4x4"/></accessor></technique_common>\n'
        '        </source>\n'
        '        <source id="Hand_Ctrl-weights">\n'
        '          <float_array id="Hand_Ctrl-weights-array" count="1">1</float_array>\n'
        '          <technique_common><accessor source="#Hand_Ctrl-weights-array" count="1" stride="1"><param name="WEIGHT" type="float"/></accessor></technique_common>\n'
        '        </source>\n'
        '        <joints>\n'
        '          <input semantic="JOINT" source="#Hand_Ctrl-joints"/>\n'
        '          <input semantic="INV_BIND_MATRIX" source="#Hand_Ctrl-bind_poses"/>\n'
        '        </joints>\n'
        '        <vertex_weights count="%d">\n'
        '          <input semantic="JOINT" offset="0" source="#Hand_Ctrl-joints"/>\n'
        '          <input semantic="WEIGHT" offset="1" source="#Hand_Ctrl-weights"/>\n'
        '          <vcount>%s</vcount>\n'
        '          <v>%s</v>\n'
        '        </vertex_weights>\n'
        '      </skin>\n'
        '    </controller>\n'
        '  </library_controllers>\n'
    ) % (geom_id, n, joint_names, n, n*16, " ".join(inv_bind), n, len(pair_joint_idx), vcount, v)

def bone_node_xml(bones, name, parent_pos):
    """Recursive node XML for the JOINT hierarchy (local translates)."""
    b = next(x for x in bones if x["name"] == name)
    pos = b["pos"]
    rel = vsub(pos, parent_pos)
    children = [x for x in bones if x["parent"] == name]
    inner = '        <node id="%s" name="%s" type="JOINT"><translate sid="translate">%s %s %s</translate>\n%s' % (
        name, name, fmt(rel[0]), fmt(rel[1]), fmt(rel[2]),
        "".join(bone_node_xml(bones, c["name"], pos) for c in children))
    return inner + "        </node>\n"

def make_scene_xml(bones, material_symbol):
    root = next(x for x in bones if x["name"] == "Hand_Root")
    rp = root["pos"]
    root_child = "".join(bone_node_xml(bones, c["name"], rp) for c in [x for x in bones if x["parent"] == "Hand_Root"])
    root_xml = (
        '      <node id="Hand_Root" name="Hand_Root" type="JOINT">\n'
        '        <translate sid="translate">%s %s %s</translate>\n%s'
        '      </node>\n'
    ) % (fmt(rp[0]), fmt(rp[1]), fmt(rp[2]), root_child)
    mesh = (
        '      <node id="Hand_Mesh" name="Hand_Mesh">\n'
        '        <instance_controller url="#Hand_Ctrl">\n'
        '          <skeleton>#Hand_Root</skeleton>\n'
        '          <bind_material><technique_common><instance_material symbol="%s" target="#%s"/></technique_common></bind_material>\n'
        '        </instance_controller>\n'
        '      </node>\n'
    ) % (material_symbol, material_symbol)
    return root_xml + mesh

# ---------------------------------------------------------------------------
# Main generation
# ---------------------------------------------------------------------------

def generate(src_path, out_path, label):
    print("=== %s ===" % label)
    positions, tri_idx, geom_id, material, normals = extract_geometry(src_path)
    print("geometry id: %s, positions: %d, triangles: %d, normals: %d, material: %s" %
          (geom_id, len(positions), len(tri_idx) // 3, len(normals), material))

    skel = compute_skeleton(positions, tri_idx, normals)
    bbox_min, bbox_max = skel["bbox"]
    print("bbox: x[%.2f, %.2f] y[%.2f, %.2f] z[%.2f, %.2f]" % (bbox_min[0], bbox_max[0], bbox_min[1], bbox_max[1], bbox_min[2], bbox_max[2]))
    print("pca axis: (%.3f, %.3f, %.3f)" % skel["pca_axis"])
    print("hand_min_x (rigid boundary): %.2f" % skel["hand_min_x"])
    print("palm center: (%.2f, %.2f, %.2f)" % skel["palm_center"])
    print("wrist point: (%.2f, %.2f, %.2f)" % skel["wrist"])
    print("spread axis: (%.3f, %.3f, %.3f)" % skel["spread_axis"])
    print("flexion axis: (%d, %d, %d) sign %+.0f" % (skel["flexion_axis"][0], skel["flexion_axis"][1], skel["flexion_axis"][2], skel["flexion_sign"]))
    print("palm front: (%.2f, %.2f, %.2f)" % skel["palm_front"])

    bones = skel["bones"]
    print("bones:")
    for b in bones:
        print("  %-10s <- %-10s (%.2f, %.2f, %.2f)" % (b["name"], str(b["parent"]), b["pos"][0], b["pos"][1], b["pos"][2]))

    for name, base, tip in skel["fingers"]:
        print("finger %-6s base (%.2f,%.2f,%.2f) tip (%.2f,%.2f,%.2f)" % (name, base[0], base[1], base[2], tip[0], tip[1], tip[2]))

    pairs = assign_weights(positions, bones, skel["hand_min_x"])
    from collections import Counter
    dist = Counter(pairs)
    print("weight distribution (joint idx: vertices):")
    for j in sorted(dist):
        print("  %2d (%s): %d" % (j, bones[j]["name"], dist[j]))

    text = open(src_path, encoding="utf-8").read()

    controller_xml = make_controller_xml(geom_id, bones, pairs)
    text = text.replace("<library_visual_scenes>", controller_xml + "<library_visual_scenes>", 1)

    # Replace the geometry-instance node with the skeleton + controller instance
    node_re = re.compile(
        r'<node[^>]*>\s*<matrix[^>]*>[^<]*</matrix>\s*<instance_geometry[^>]*url="#%s"[^>]*>.*?</node>' % re.escape(geom_id),
        re.DOTALL)
    m = node_re.search(text)
    if not m:
        raise RuntimeError("could not find the geometry node in %s" % src_path)
    scene_xml = make_scene_xml(bones, material)
    text = text[:m.start()] + scene_xml + text[m.end():]

    with open(out_path, "w", encoding="utf-8") as f:
        f.write(text)
    print("wrote %s" % out_path)

    return skel

def main():
    rs = generate(RIGHT_SRC, RIGHT_OUT, "RIGHT HAND")
    ls = generate(LEFT_SRC, LEFT_OUT, "LEFT HAND")
    print()
    print("C++ constants (paste into PlayerHands.cpp):")
    for label, skel in (("RIGHT", rs), ("LEFT", ls)):
        a = skel["flexion_axis"]
        print('  // %s: finger flexion axis = (%d, %d, %d)' % (label, a[0], a[1], a[2]))
    print()
    print("  // Thumb flexion axis: tilted ~27 deg from +Y toward +Z (right) / -Z (left),")
    print("  // so the thumb curls across the palm instead of straight up.")
    print("  // RIGHT: (0.0000, 0.8910, 0.4540)   LEFT: (0.0000, 0.8910, -0.4540)")
    print()
    print("Animation angles are CUMULATIVE totals per bone (own rotation = diff):")
    print("  grip:     Middle/Ring 60/120/165 (own 60/60/45); Little 55/110/150 (55/55/40); Thumb 40/75 (40/35)")
    print("  trigger:  Index 55/110/155 (own 55/55/45)")
    print("  index:    Index 60/120/165 (own 60/60/45)")

if __name__ == "__main__":
    main()