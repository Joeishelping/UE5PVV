"""Procedural rebuild of the bean character: smooth, 1.8 m, rigged with UE-mannequin bone names, exported as glTF (.glb).
glTF space: Y up, metres, character faces +Z, character's left = +X."""
import json, struct, numpy as np

# ---------------------------------------------------------------- skeleton
BONES = [  # name, parent, head position
 ("root", None, (0, 0, 0)),
 ("pelvis", "root", (0, .45, 0)),
 ("spine_01", "pelvis", (0, .62, 0)),
 ("spine_02", "spine_01", (0, .85, 0)),
 ("spine_03", "spine_02", (0, 1.08, 0)),
 ("neck_01", "spine_03", (0, 1.30, 0)),
 ("head", "neck_01", (0, 1.45, 0)),
]
for s, x in (("l", 1), ("r", -1)):
    BONES += [
     (f"clavicle_{s}", "spine_03", (.06*x, 1.20, 0)),
     (f"upperarm_{s}", f"clavicle_{s}", (.24*x, 1.18, 0)),
     (f"lowerarm_{s}", f"upperarm_{s}", (.40*x, .92, .02)),
     (f"hand_{s}", f"lowerarm_{s}", (.46*x, .64, .04)),
     (f"thigh_{s}", "pelvis", (.12*x, .42, 0)),
     (f"calf_{s}", f"thigh_{s}", (.13*x, .23, .01)),
     (f"foot_{s}", f"calf_{s}", (.13*x, .08, 0)),
     (f"ball_{s}", f"foot_{s}", (.13*x, .03, .11)),
    ]
BI = {b[0]: i for i, b in enumerate(BONES)}
POS = {b[0]: np.array(b[2], float) for b in BONES}
TAIL_OVERRIDE = {"head": (0, 1.80, 0), "root": (0, .45, 0), "pelvis": (0, .62, 0), "spine_03": (0, 1.30, 0)}
def tail(name):
    if name in TAIL_OVERRIDE: return np.array(TAIL_OVERRIDE[name], float)
    kids = [b for b in BONES if b[1] == name]
    if kids: return POS[kids[0][0]]
    if name.startswith("hand"): return POS[name] + np.array([0, -.1, .01])
    if name.startswith("ball"): return POS[name] + np.array([0, 0, .06])
    return POS[name] + np.array([0, .05, 0])

def seg_dist(P, a, b):
    ab = b - a; t = np.clip(((P - a) @ ab) / max(ab @ ab, 1e-9), 0, 1)
    return np.linalg.norm(P - (a + t[:, None] * ab), axis=1)

def skin(V, bones, power=5):
    D = np.stack([seg_dist(V, POS[b], tail(b)) for b in bones], 1)
    W = 1.0 / (D + .015) ** power
    idx = np.argsort(-W, 1)[:, :4]
    J = np.zeros((len(V), 4), int); Wt = np.zeros((len(V), 4))
    for k in range(min(4, len(bones))):
        J[:, k] = [BI[bones[i]] for i in idx[:, k]]
        Wt[:, k] = W[np.arange(len(V)), idx[:, k]]
    Wt[Wt < Wt[:, :1] * .02] = 0
    Wt /= Wt.sum(1, keepdims=True)
    return J, Wt

# ---------------------------------------------------------------- geometry helpers
def grid(rings, start_pole=None, end_pole=None):
    """rings: list of (S,3) arrays. Returns V, F (closed around)."""
    S = len(rings[0]); V = list(np.vstack(rings)); F = []
    for i in range(len(rings) - 1):
        for j in range(S):
            a, b = i*S + j, i*S + (j+1) % S; c, d = a + S, b + S
            F += [(a, c, b), (b, c, d)]
    if start_pole is not None:
        p = len(V); V.append(np.array(start_pole, float))
        F += [(p, j, (j+1) % S) for j in range(S)]
    if end_pole is not None:
        p = len(V); V.append(np.array(end_pole, float)); o = (len(rings)-1)*S
        F += [(p, o + (j+1) % S, o + j) for j in range(S)]
    return np.array(V), np.array(F)

def orient(V, F):
    vol = np.einsum('ij,ij->i', V[F[:, 0]], np.cross(V[F[:, 1]], V[F[:, 2]])).sum()
    return F if vol > 0 else F[:, ::-1]

def catmull(P, n_per=8):
    P = np.asarray(P, float); P = np.vstack([2*P[0]-P[1], P, 2*P[-1]-P[-2]]); out = []
    for i in range(1, len(P)-2):
        p0, p1, p2, p3 = P[i-1:i+3]
        for t in np.linspace(0, 1, n_per, endpoint=False):
            out.append(.5*((2*p1) + (-p0+p2)*t + (2*p0-5*p1+4*p2-p3)*t*t + (-p0+3*p1-3*p2+p3)*t**3))
    out.append(P[-2]); return np.array(out)

def revolve(ys, rs, S=32, depth=1.0, center=(0, 0, 0)):
    ys = catmull(ys, 6)[:, 0] if False else ys
    cx, _, cz = center; th = np.linspace(0, 2*np.pi, S, endpoint=False)
    rings = [np.stack([cx + r*np.cos(th), np.full(S, y), cz + depth*r*np.sin(th)], 1) for y, r in zip(ys[1:-1], rs[1:-1])]
    return grid(rings, (cx, ys[0], cz), (cx, ys[-1], cz))

def smooth_profile(ys, rs, n=6):
    pts = catmull(np.stack([ys, rs], 1), n); pts[:, 1] = np.maximum(pts[:, 1], 0); return pts[:, 0], pts[:, 1]

def tube(path, radii, S=20, n_per=6, caps=True):
    P = catmull(path, n_per); R = np.interp(np.linspace(0, 1, len(P)), np.linspace(0, 1, len(radii)), radii)
    T = np.gradient(P, axis=0); T /= np.linalg.norm(T, axis=1, keepdims=True)
    up = np.array([0, 0, 1.]) if abs(T[0] @ [0, 0, 1]) < .9 else np.array([1., 0, 0])
    N = np.cross(T[0], up); N /= np.linalg.norm(N); frames = []
    for i in range(len(P)):
        if i: N = N - (N @ T[i]) * T[i]; N /= np.linalg.norm(N)
        frames.append((N, np.cross(T[i], N)))
    th = np.linspace(0, 2*np.pi, S, endpoint=False); rings = []
    def ring(c, r, n, b): return c + r*(np.outer(np.cos(th), n) + np.outer(np.sin(th), b))
    if caps:  # hemispherical start cap
        for a in np.linspace(np.pi/2, 0, 5)[1:-1]:
            rings.append(ring(P[0] - T[0]*R[0]*np.sin(a), R[0]*np.cos(a), *frames[0]))
    for i in range(len(P)): rings.append(ring(P[i], R[i], *frames[i]))
    if caps:
        for a in np.linspace(0, np.pi/2, 5)[1:-1]:
            rings.append(ring(P[-1] + T[-1]*R[-1]*np.sin(a), R[-1]*np.cos(a), *frames[-1]))
    return grid(rings, P[0] - T[0]*R[0] if caps else None, P[-1] + T[-1]*R[-1] if caps else None)

def ellipsoid(c, r, S=24, rings=14):
    c = np.array(c, float); ph = np.linspace(-np.pi/2, np.pi/2, rings+1)[1:-1]; th = np.linspace(0, 2*np.pi, S, endpoint=False)
    R = [c + np.stack([r[0]*np.cos(p)*np.cos(th), np.full(S, r[1]*np.sin(p)), r[2]*np.cos(p)*np.sin(th)], 1) for p in ph]
    return grid(R, c - [0, r[1], 0], c + [0, r[1], 0])

def normals(V, F):
    N = np.zeros_like(V); fn = np.cross(V[F[:, 1]] - V[F[:, 0]], V[F[:, 2]] - V[F[:, 0]])
    for k in range(3): np.add.at(N, F[:, k], fn)
    return N / np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)

# ---------------------------------------------------------------- glb writer
def write_glb(path, parts, materials):
    """parts: list of (V, F, J, W, material_index). One skinned mesh, one primitive per part."""
    buf = bytearray(); views = []; accs = []
    def add(arr, comp, typ, target=None, minmax=False):
        nonlocal buf
        while len(buf) % 4: buf.append(0)
        off = len(buf); b = arr.tobytes(); buf += b
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(b)}
        if target: v["target"] = target
        views.append(v)
        a = {"bufferView": len(views)-1, "componentType": comp, "count": len(arr), "type": typ}
        if minmax: a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accs.append(a); return len(accs)-1
    prims = []
    for V, F, J, W, m in parts:
        F = orient(V, F)
        p = add(V.astype(np.float32), 5126, "VEC3", 34962, True)
        n = add(normals(V, F).astype(np.float32), 5126, "VEC3", 34962)
        uv = np.stack([(np.arctan2(V[:, 2], V[:, 0]) / (2*np.pi)) % 1, 1 - V[:, 1] / 2], 1).astype(np.float32)
        t = add(uv, 5126, "VEC2", 34962)
        j = add(J.astype(np.uint16), 5123, "VEC4", 34962)
        w = add(W.astype(np.float32), 5126, "VEC4", 34962)
        i = add(F.astype(np.uint32).reshape(-1), 5125, "SCALAR", 34963)
        prims.append({"attributes": {"POSITION": p, "NORMAL": n, "TEXCOORD_0": t, "JOINTS_0": j, "WEIGHTS_0": w}, "indices": i, "material": m})
    ibm = np.stack([np.array([[1,0,0,0],[0,1,0,0],[0,0,1,0],[-x,-y,-z,1]], np.float32).reshape(-1) for x, y, z in (POS[b[0]] for b in BONES)])
    ibm_acc = add(ibm, 5126, "MAT4")
    nodes = []
    for name, parent, pos in BONES:
        local = POS[name] - (POS[parent] if parent else 0)
        node = {"name": name, "translation": [float(v) for v in local]}
        kids = [BI[b[0]] for b in BONES if b[1] == name]
        if kids: node["children"] = kids
        nodes.append(node)
    mesh_node = len(nodes)
    nodes.append({"name": path.split('/')[-1].replace('.glb', ''), "mesh": 0, "skin": 0})
    gltf = {
        "asset": {"version": "2.0", "generator": "PVV procedural character"},
        "scene": 0, "scenes": [{"nodes": [0, mesh_node]}], "nodes": nodes,
        "meshes": [{"name": nodes[mesh_node]["name"], "primitives": prims}],
        "skins": [{"name": "SK_PVV", "joints": list(range(len(BONES))), "inverseBindMatrices": ibm_acc, "skeleton": 0}],
        "materials": [{"name": n, "doubleSided": ds, "pbrMetallicRoughness": {"baseColorFactor": list(c) + [1.0], "metallicFactor": met, "roughnessFactor": rough}} for n, c, met, rough, ds in materials],
        "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}],
    }
    while len(buf) % 4: buf.append(0)
    js = json.dumps(gltf, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    out = struct.pack('<III', 0x46546C67, 2, 12 + 8 + len(js) + 8 + len(buf))
    out += struct.pack('<II', len(js), 0x4E4F534A) + js + struct.pack('<II', len(buf), 0x004E4942) + bytes(buf)
    open(path, 'wb').write(out)

def part(VF, bones, mat):
    V, F = VF; J, W = skin(V, bones); return (V, F, J, W, mat)

# ---------------------------------------------------------------- body
SPINE = ["pelvis", "spine_01", "spine_02", "spine_03", "neck_01"]
body = []
ys, rs = smooth_profile([.36, .42, .55, .75, .95, 1.10, 1.22, 1.31, 1.36], [0, .12, .22, .28, .29, .27, .22, .14, 0])
body.append(part(revolve(ys, rs, 40, .85), SPINE, 1))                                        # torso / top
ys, rs = smooth_profile([.25, .28, .34, .45, .56, .63, .65], [0, .16, .25, .292, .30, .30, 0])
body.append(part(revolve(ys, rs, 40, .86), ["pelvis", "spine_01", "thigh_l", "thigh_r"], 2))   # shorts
body.append(part(ellipsoid((0, 1.52, 0), (.285, .285, .285), 40, 24), ["head"], 0))          # head
for s, x in (("l", 1), ("r", -1)):
    arm = [(.17*x, 1.20, 0), (.24*x, 1.18, 0), (.33*x, 1.07, .01), (.40*x, .92, .02), (.44*x, .78, .03), (.46*x, .66, .04)]
    body.append(part(tube(arm, [.072, .07, .068, .064, .06, .056]), [f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"], 0))
    body.append(part(ellipsoid((.47*x, .585, .05), (.075, .085, .07), 24, 14), [f"hand_{s}"], 0))
    leg = [(.12*x, .42, 0), (.125*x, .32, .005), (.13*x, .22, .01), (.13*x, .14, .005), (.13*x, .09, 0)]
    body.append(part(tube(leg, [.09, .085, .077, .07, .066]), [f"thigh_{s}", f"calf_{s}", f"foot_{s}"], 0))
    body.append(part(ellipsoid((.13*x, .055, .055), (.088, .055, .145), 24, 12), [f"foot_{s}", f"ball_{s}"], 3))

MATS = [  # name, colour, metallic, roughness, double sided
    ("M_Skin", (.85, .62, .48), 0, .7, False), ("M_Top", (.85, .78, .35), 0, .8, False),
    ("M_Bottom", (.45, .35, .8), 0, .8, False), ("M_Shoes", (.18, .12, .08), 0, .6, False)]
write_glb("SK_PVV_Body.glb", body, MATS)

# ---------------------------------------------------------------- accessories (same skeleton, all weighted to head)
HC = np.array([0, 1.52, 0]); HR = .285
def dome(r, y0_frac, S=40, rings=14, scale_y=1.0, center=HC):
    """Shell cap from latitude asin(y0_frac) up to the pole."""
    lo = np.arcsin(y0_frac); ph = np.linspace(lo, np.pi/2, rings)[:-1]; th = np.linspace(0, 2*np.pi, S, endpoint=False)
    R = [center + np.stack([r*np.cos(p)*np.cos(th), np.full(S, r*scale_y*np.sin(p)), r*np.cos(p)*np.sin(th)], 1) for p in ph]
    return grid(R, None, center + [0, r*scale_y, 0])
def solid_dome(r_out, r_in, y0, S=40):
    Vo, Fo = dome(r_out, y0, S); Vi, Fi = dome(r_in, y0, S)
    V = np.vstack([Vo, Vi]); F = np.vstack([Fo, Fi[:, ::-1] + len(Vo)])
    # stitch rims
    rim = [(j, (j+1) % S, len(Vo)+j, len(Vo)+(j+1) % S) for j in range(S)]
    F = np.vstack([F] + [[(a, c, b), (b, c, d)] for a, b, c, d in rim]); return V, F
def merge(*vfs):
    V = []; F = []; o = 0
    for v, f in vfs: V.append(v); F.append(f + o); o += len(v)
    return np.vstack(V), np.vstack(F)
def head_part(VF, mat): V, F = VF; return part((V, F), ["head"], mat)

# Viking helmet: dome + rim + horns
helmet = merge(solid_dome(HR + .03, HR + .005, .05),
               tube([(np.cos(t)*(HR+.035), HC[1] + .015, np.sin(t)*(HR+.035)) for t in np.linspace(0, 2*np.pi, 33)][:-1] + [(HR+.035, HC[1]+.015, 0)], [.03]*33, S=10, n_per=2, caps=False))
horns = []
for x in (1, -1):
    horns.append(tube([(.22*x, 1.66, 0), (.33*x, 1.72, 0), (.42*x, 1.82, .02), (.45*x, 1.95, .04)], [.065, .05, .03, .006], S=16))
write_glb("SK_PVV_VikingHelmet.glb", [head_part(helmet, 0), head_part(merge(*horns), 1)],
          [("M_HelmetMetal", (.55, .55, .58), .9, .35, True), ("M_Horn", (.92, .87, .72), 0, .6, False)])

beard = merge(ellipsoid((0, 1.36, .19), (.22, .17, .12), 32, 16), ellipsoid((0, 1.22, .21), (.13, .12, .09), 24, 12))
write_glb("SK_PVV_Beard.glb", [head_part(beard, 0)], [("M_Beard", (.75, .32, .08), 0, .9, False)])

# Pirate tricorn: rounded crown + brim turned up into a triangle with a point at the front
th = np.linspace(0, 2*np.pi, 90, endpoint=False)
crown = solid_dome(.235, .22, .0, 40)
crown = (crown[0] * [1, .85, 1] + [0, 1.69 - 1.52*.85, 0], crown[1])
tri = np.maximum(0, np.cos(3*(th - np.pi/2)))  # 1 at the three corners (front, back-left, back-right)
r_out = .30 + .14*tri**2
rings = []
for t in np.linspace(0, 1, 9):
    r = .19 + (r_out - .19)*t
    lift = .16*t**1.3 * (1 - .65*tri**2)  # sides fold up steeply, corners stay lower and pointy
    rings.append(np.stack([r*np.cos(th), 1.69 + lift, r*np.sin(th)], 1))
bv, bf = grid(rings); bv2 = bv + [0, .014, 0]
brim = (np.vstack([bv, bv2]), np.vstack([bf, bf[:, ::-1] + len(bv)]))
trim = tube([tuple(rr) for rr in rings[-1][::2]] + [tuple(rings[-1][0])], [.012]*46, S=8, n_per=2, caps=False)
write_glb("SK_PVV_PirateHat.glb", [head_part(crown, 0), head_part(brim, 0), head_part(trim, 1)],
          [("M_HatFelt", (.08, .07, .07), 0, .9, True), ("M_HatTrim", (.85, .68, .2), .8, .4, False)])

bandana = merge(solid_dome(HR + .015, HR + .002, .18),
                tube([(0, 1.62, -.29), (.04, 1.55, -.36), (.06, 1.45, -.38)], [.03, .025, .012], S=10),
                tube([(0, 1.62, -.29), (-.05, 1.56, -.35), (-.08, 1.47, -.36)], [.03, .025, .012], S=10),
                ellipsoid((0, 1.62, -.30), (.05, .04, .035), 16, 8))
write_glb("SK_PVV_Bandana.glb", [head_part(bandana, 0)], [("M_Bandana", (.75, .08, .08), 0, .8, True)])

ex, ey = -.10, 1.57; ez = np.sqrt(HR**2 - ex**2 - (ey-HC[1])**2) + .01
patch = ellipsoid((ex, ey, ez), (.065, .055, .015), 20, 8)
def strap_pt(t):
    y = ey + .085*np.cos(t - np.radians(15))  # tilted band: eye level at the patch, high at the left temple
    r = np.sqrt(max(HR**2 - (y - HC[1])**2, 0)) + .008
    return (np.cos(t)*r, y, np.sin(t)*r)
tp = np.arctan2(ez, ex)
strap = tube([strap_pt(t) for t in np.linspace(tp + .25, tp + 2*np.pi - .25, 40)], [.011]*40, S=8, n_per=2, caps=True)
write_glb("SK_PVV_Eyepatch.glb", [head_part(patch, 0), head_part(strap, 0)], [("M_Eyepatch", (.03, .03, .03), 0, .7, False)])
print("written")
