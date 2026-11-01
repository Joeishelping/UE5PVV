"""PVV character kit v2: body with face + morph sliders, hairstyles, outfits. All skinned to one skeleton.
glTF space: Y up, metres, faces +Z, character's left = +X."""
import json, struct, numpy as np

# ================================================================ skeleton
BONES = [("root", None, (0, 0, 0)), ("pelvis", "root", (0, .45, 0)), ("spine_01", "pelvis", (0, .62, 0)),
         ("spine_02", "spine_01", (0, .85, 0)), ("spine_03", "spine_02", (0, 1.08, 0)),
         ("neck_01", "spine_03", (0, 1.30, 0)), ("head", "neck_01", (0, 1.45, 0))]
for s, x in (("l", 1), ("r", -1)):
    BONES += [(f"clavicle_{s}", "spine_03", (.06*x, 1.20, 0)), (f"upperarm_{s}", f"clavicle_{s}", (.24*x, 1.18, 0)),
              (f"lowerarm_{s}", f"upperarm_{s}", (.40*x, .92, .02)), (f"hand_{s}", f"lowerarm_{s}", (.46*x, .64, .04)),
              (f"thigh_{s}", "pelvis", (.12*x, .42, 0)), (f"calf_{s}", f"thigh_{s}", (.13*x, .23, .01)),
              (f"foot_{s}", f"calf_{s}", (.13*x, .08, 0)), (f"ball_{s}", f"foot_{s}", (.13*x, .03, .11))]
BI = {b[0]: i for i, b in enumerate(BONES)}
POS = {b[0]: np.array(b[2], float) for b in BONES}
TAILS = {"head": (0, 1.8, 0), "root": (0, .45, 0), "pelvis": (0, .62, 0), "spine_03": (0, 1.3, 0)}
def tail(n):
    if n in TAILS: return np.array(TAILS[n], float)
    kids = [b for b in BONES if b[1] == n]
    if kids: return POS[kids[0][0]]
    return POS[n] + (np.array([0, -.1, .01]) if n.startswith("hand") else np.array([0, 0, .06]) if n.startswith("ball") else np.array([0, .05, 0]))

# ================================================================ body dimensions
HC = np.array([0, 1.52, 0.]); HR = .285
DEPTH = .85
def catmull(P, n=8):
    P = np.asarray(P, float); P = np.vstack([2*P[0]-P[1], P, 2*P[-1]-P[-2]]); out = []
    for i in range(1, len(P)-2):
        p0, p1, p2, p3 = P[i-1:i+3]
        for t in np.linspace(0, 1, n, endpoint=False):
            out.append(.5*((2*p1)+(-p0+p2)*t+(2*p0-5*p1+4*p2-p3)*t*t+(-p0+3*p1-3*p2+p3)*t**3))
    out.append(P[-2]); return np.array(out)
def smooth_profile(ys, rs, n=6):
    p = catmull(np.stack([ys, rs], 1), n); p[:, 1] = np.maximum(p[:, 1], 0); return p[:, 0], p[:, 1]
TY, TR = smooth_profile([.36, .42, .55, .75, .95, 1.10, 1.22, 1.31, 1.36], [0, .12, .22, .28, .29, .27, .22, .14, 0])
SY, SR = smooth_profile([.25, .28, .34, .45, .56, .63, .65], [0, .16, .25, .292, .30, .30, 0])
def torso_r(y): return np.interp(y, TY, TR)
def hips_r(y): return np.interp(y, SY, SR)
def env_r(y): return np.maximum(torso_r(y), np.where((y > .25) & (y < .65), hips_r(y), 0))
ARM = {s: np.array([(.17*x, 1.20, 0), (.24*x, 1.18, 0), (.33*x, 1.07, .01), (.40*x, .92, .02), (.44*x, .78, .03), (.46*x, .66, .04)]) for s, x in (("l", 1), ("r", -1))}
ARM_R = [.072, .07, .068, .064, .06, .056]
LEG = {s: np.array([(.12*x, .42, 0), (.125*x, .32, .005), (.13*x, .22, .01), (.13*x, .14, .005), (.13*x, .09, 0)]) for s, x in (("l", 1), ("r", -1))}
LEG_R = [.09, .085, .077, .07, .066]
HAND_C = {s: np.array([.47*x, .585, .05]) for s, x in (("l", 1), ("r", -1))}
EYE_C = {s: np.array([.095*x, 1.575, .255]) for s, x in (("l", 1), ("r", -1))}
NOSE_B = np.array([0, 1.50, .25]); LIP_C = np.array([0, 1.415, .24]); EAR_ROOT = {s: np.array([.27*x, 1.53, -.01]) for s, x in (("l", 1), ("r", -1))}

# ================================================================ geometry helpers
def grid(rings, start_pole=None, end_pole=None, wrap=True):
    S = len(rings[0]); V = list(np.vstack(rings)); F = []
    for i in range(len(rings)-1):
        for j in range(S if wrap else S-1):
            a, b = i*S+j, i*S+(j+1) % S; c, d = a+S, b+S; F += [(a, c, b), (b, c, d)]
    if start_pole is not None:
        p = len(V); V.append(np.array(start_pole, float)); F += [(p, j, (j+1) % S) for j in range(S)]
    if end_pole is not None:
        p = len(V); V.append(np.array(end_pole, float)); o = (len(rings)-1)*S; F += [(p, o+(j+1) % S, o+j) for j in range(S)]
    return np.array(V, float), np.array(F, int)
def merge(*vfs):
    V, F, o = [], [], 0
    for v, f in vfs: V.append(v); F.append(f+o); o += len(v)
    return np.vstack(V), np.vstack(F)
TH = lambda S: np.linspace(0, 2*np.pi, S, endpoint=False)
def revolve_profile(ys, rs, S=40, depth=DEPTH, close_bottom=False, close_top=False, cx=0.):
    th = TH(S); rings = [np.stack([cx+r*np.cos(th), np.full(S, y), depth*r*np.sin(th)], 1) for y, r in zip(ys, rs)]
    return grid(rings, (cx, ys[0], 0) if close_bottom else None, (cx, ys[-1], 0) if close_top else None)
def tube(path, radii, S=16, n=6, caps=True, mod=None):
    P = catmull(path, n); R = np.interp(np.linspace(0, 1, len(P)), np.linspace(0, 1, len(radii)), radii)
    if mod is not None: R = R*mod(np.linspace(0, 1, len(P)))
    T = np.gradient(P, axis=0); T /= np.linalg.norm(T, axis=1, keepdims=True)
    up = np.array([0, 0, 1.]) if abs(T[0] @ [0, 0, 1]) < .9 else np.array([1., 0, 0])
    N = np.cross(T[0], up); N /= np.linalg.norm(N); fr = []
    for i in range(len(P)):
        if i: N = N-(N @ T[i])*T[i]; N /= np.linalg.norm(N)
        fr.append((N, np.cross(T[i], N)))
    th = TH(S); ring = lambda c, r, nn, bb: c+r*(np.outer(np.cos(th), nn)+np.outer(np.sin(th), bb)); rings = []
    if caps:
        for a in np.linspace(np.pi/2, 0, 5)[1:-1]: rings.append(ring(P[0]-T[0]*R[0]*np.sin(a), R[0]*np.cos(a), *fr[0]))
    for i in range(len(P)): rings.append(ring(P[i], R[i], *fr[i]))
    if caps:
        for a in np.linspace(0, np.pi/2, 5)[1:-1]: rings.append(ring(P[-1]+T[-1]*R[-1]*np.sin(a), R[-1]*np.cos(a), *fr[-1]))
    return grid(rings, P[0]-T[0]*R[0] if caps else None, P[-1]+T[-1]*R[-1] if caps else None)
def ellipsoid(c, r, S=20, rings=12):
    c = np.array(c, float); ph = np.linspace(-np.pi/2, np.pi/2, rings+1)[1:-1]; th = TH(S)
    R = [c+np.stack([r[0]*np.cos(p)*np.cos(th), np.full(S, r[1]*np.sin(p)), r[2]*np.cos(p)*np.sin(th)], 1) for p in ph]
    return grid(R, c-[0, r[1], 0], c+[0, r[1], 0])
def ring_torus(y, R, r, S=48, s=10, depth=DEPTH, mod=None):
    pts = [(R*np.cos(t), y, depth*R*np.sin(t)) for t in np.linspace(0, 2*np.pi, S+1)]
    V, F = tube(pts, [r], S=s, n=1, caps=False, mod=mod); return V, F
def sheet(P):  # P (rows, cols, 3) open grid
    r, c, _ = P.shape; V = P.reshape(-1, 3); F = []
    for i in range(r-1):
        for j in range(c-1):
            a = i*c+j; F += [(a, a+c, a+1), (a+1, a+c, a+c+1)]
    return V, np.array(F)
def sph(th, ph, r, c=HC):  # th around Y (front = +pi/2), ph latitude
    return c+np.stack([r*np.cos(ph)*np.cos(th), r*np.sin(ph), r*np.cos(ph)*np.sin(th)], -1)
def cap(r_out, hairline, S=56, rings=14, r_in=HR+.004):
    """Solid shell over the scalp from a hairline (y per theta) to the crown. r_out(th, ph) -> radius."""
    th = TH(S); ph0 = np.arcsin(np.clip((hairline(th)-HC[1])/HR, -.99, .99)); k = np.linspace(0, 1, rings)[:-1]
    PH = ph0[None, :]+(np.pi/2-ph0[None, :])*k[:, None]; TT = np.broadcast_to(th, PH.shape)
    out = [sph(TT[i], PH[i], r_out(TT[i], PH[i])) for i in range(len(k))]
    inn = [sph(TT[i], PH[i], np.full(S, r_in)) for i in range(len(k))]
    Vo, Fo = grid(out, None, HC+[0, r_out(np.array([0.]), np.array([np.pi/2]))[0], 0])
    Vi, Fi = grid(inn, None, HC+[0, r_in, 0])
    rim = [(j, (j+1) % S, len(Vo)+j, len(Vo)+(j+1) % S) for j in range(S)]
    F = np.vstack([Fo, Fi[:, ::-1]+len(Vo)]+[[(a, c, b), (b, c, d)] for a, b, c, d in rim])
    return np.vstack([Vo, Vi]), F
def surface_pt(th, y, off):  # point on the torso/hips envelope
    r = env_r(np.array([y]))[0]+off; return np.array([r*np.cos(th), y, DEPTH*r*np.sin(th)])
FRONT, BACK = np.pi/2, -np.pi/2
def hairline_std(th, front=1.68, back=1.43): return (front+back)/2+(front-back)/2*np.sin(th)

# ================================================================ parts & tags
class Part:
    def __init__(s, VF, tag, mat): s.V, s.F, s.tag, s.mat = VF[0], VF[1], tag, mat
HEADISH = {"head", "eye", "pupil", "brow", "nose", "lips", "ear", "hair", "hat", "facehair", "mustache", "patch"}

def side_of(tag): return tag[-1] if tag[-2:] in ("_l", "_r") else None
def seg_dist(P, a, b):
    ab = b-a; t = np.clip(((P-a) @ ab)/max(ab @ ab, 1e-9), 0, 1); return np.linalg.norm(P-(a+t[:, None]*ab), axis=1)
def dist_skin(V, bones, power=5):
    D = np.stack([seg_dist(V, POS[b], tail(b)) for b in bones], 1); W = 1/(D+.015)**power
    return [bones[i] for i in range(len(bones))], W
def skin(part):
    V, tag = part.V, part.tag; s = side_of(tag); n = len(V)
    if tag in HEADISH:
        w = np.clip((V[:, 1]-1.15)/.17, 0, 1); w = w*w*(3-2*w)
        bones = ["head", "spine_03"]; W = np.stack([w, 1-w], 1)
    elif tag in ("torso",): bones, W = dist_skin(V, ["pelvis", "spine_01", "spine_02", "spine_03", "neck_01"])
    elif tag == "hips": bones, W = dist_skin(V, ["pelvis", "spine_01", "thigh_l", "thigh_r"])
    elif tag == "skirt": bones, W = dist_skin(V, ["pelvis", "thigh_l", "thigh_r", "calf_l", "calf_r"], 3)
    elif tag == "shawl": bones, W = dist_skin(V, ["spine_02", "spine_03", "neck_01", "clavicle_l", "clavicle_r"])
    elif tag.startswith("arm"): bones, W = dist_skin(V, [f"upperarm_{s}", f"lowerarm_{s}", f"hand_{s}"])
    elif tag.startswith("hand"): bones, W = [f"hand_{s}"], np.ones((n, 1))
    elif tag.startswith("leg"): bones, W = dist_skin(V, [f"thigh_{s}", f"calf_{s}", f"foot_{s}"])
    elif tag.startswith("foot"): bones, W = dist_skin(V, [f"foot_{s}", f"ball_{s}"])
    else: raise ValueError(tag)
    idx = np.argsort(-W, 1)[:, :4]; J = np.zeros((n, 4), int); Wt = np.zeros((n, 4))
    for k in range(min(4, len(bones))):
        J[:, k] = [BI[bones[i]] for i in idx[:, k]]; Wt[:, k] = W[np.arange(n), idx[:, k]]
    Wt[Wt < Wt[:, :1]*.02] = 0; Wt /= Wt.sum(1, keepdims=True); return J, Wt

# ================================================================ morph sliders (spatial fields, so every layer follows)
def closest_on_path(V, P):
    best = np.full(len(V), 1e9); C = np.zeros_like(V); T = np.zeros(len(V)); L = np.r_[0, np.cumsum(np.linalg.norm(np.diff(P, axis=0), axis=1))]
    for i in range(len(P)-1):
        a, b = P[i], P[i+1]; ab = b-a; t = np.clip(((V-a) @ ab)/(ab @ ab), 0, 1); c = a+t[:, None]*ab; d = np.linalg.norm(V-c, axis=1)
        m = d < best; best[m] = d[m]; C[m] = c[m]; T[m] = (L[i]+t[m]*np.linalg.norm(ab))/L[-1]
    return C, T
def smoothstep(x): x = np.clip(x, 0, 1); return x*x*(3-2*x)
def belly_g(y): return np.exp(-((y-.80)/.25)**2)
def morph_delta(name, V, tag):
    D = np.zeros_like(V); x, y, z = V[:, 0], V[:, 1], V[:, 2]; s = side_of(tag); sx = np.sign(x)
    H = tag in HEADISH
    if name == "Head_Wide" and H: D[:, 0] = x*.15
    elif name == "Head_Narrow" and H: D[:, 0] = -x*.12
    elif name == "Head_Tall" and H: D[:, 1] = (y-HC[1])*.12
    elif name == "Jaw_Wide" and H:
        f = .22*smoothstep((HC[1]-y)/HR); D[:, 0] = x*f; D[:, 2] = z*f*.25
    elif name == "Nose_Big" and tag == "nose": D = (V-NOSE_B)*.45
    elif name == "Nose_Small" and tag == "nose": D = (V-NOSE_B)*-.3
    elif name == "Nose_Wide" and tag == "nose": D[:, 0] = x*.5
    elif name == "Nose_Long" and tag == "nose": d = np.maximum(z-NOSE_B[2], 0); D[:, 2] = d*.7; D[:, 1] = -d*.25
    elif name == "Lips_Full" and tag == "lips": D[:, 1] = (y-LIP_C[1])*.55; D[:, 2] = np.maximum(z-LIP_C[2], 0)*.6
    elif name == "Lips_Thin" and tag == "lips": D[:, 1] = (y-LIP_C[1])*-.45
    elif name == "Mouth_Wide" and tag in ("lips", "mustache"): D[:, 0] = x*.35
    elif name in ("Eyes_Big", "Eyes_Small") and tag in ("eye", "pupil", "patch"):
        k = .35 if name == "Eyes_Big" else -.25
        C = np.where(sx[:, None] > 0, EYE_C["l"], EYE_C["r"]); D = (V-C)*k
    elif name == "Eyes_Apart" and tag in ("eye", "pupil", "brow", "patch"): D[:, 0] = sx*.03
    elif name == "Brow_Heavy" and tag == "brow":
        C = np.where(sx[:, None] > 0, EYE_C["l"]+[0, .07, 0], EYE_C["r"]+[0, .07, 0]); D = (V-C)*[0, .8, .5]; D[:, 1] -= .008
    elif name == "Brow_Angry" and tag == "brow": D[:, 1] = (np.abs(x)-.095)*.35
    elif name == "Ears_Big" and tag == "ear":
        C = np.where(sx[:, None] > 0, EAR_ROOT["l"], EAR_ROOT["r"]); D = (V-C)*.5
    elif name == "Belly":
        g = belly_g(y)
        if tag in ("torso", "hips", "skirt", "shawl"): D[:, 0] = x*.22*g; D[:, 2] = z*np.where(z > 0, .45, .18)*g
        elif tag.startswith(("arm", "hand")): D[:, 0] = sx*.07*g
    elif name == "Skinny" and tag in ("torso", "hips", "skirt", "shawl"):
        g = np.exp(-((y-.85)/.4)**2); D[:, 0] = -x*.2*g; D[:, 2] = -z*.2*g
    elif name == "Arms_Muscle":
        if tag.startswith("arm"):
            C, T = closest_on_path(V, ARM[s]); bump = .25+.45*np.exp(-((T-.3)/.18)**2)+.3*np.exp(-((T-.72)/.15)**2); D = (V-C)*bump[:, None]
        elif tag.startswith("hand"): D = (V-HAND_C[s])*.12
    elif name == "Legs_Thick":
        if tag.startswith("leg"): C, _ = closest_on_path(V, LEG[s]); D = (V-C)*.35
        elif tag == "skirt": f = .07*smoothstep((.5-y)/.2); D[:, 0] = x*f; D[:, 2] = z*f
        elif tag.startswith("foot"): D[:, 0] = (x-np.sign(x)*.13)*.12
    return D
MORPHS = ["Head_Wide", "Head_Narrow", "Head_Tall", "Jaw_Wide", "Nose_Big", "Nose_Small", "Nose_Wide", "Nose_Long",
          "Lips_Full", "Lips_Thin", "Mouth_Wide", "Eyes_Big", "Eyes_Small", "Eyes_Apart", "Brow_Heavy", "Brow_Angry",
          "Ears_Big", "Belly", "Skinny", "Arms_Muscle", "Legs_Thick"]

# ================================================================ glb export
def normals(V, F):
    N = np.zeros_like(V); fn = np.cross(V[F[:, 1]]-V[F[:, 0]], V[F[:, 2]]-V[F[:, 0]])
    for k in range(3): np.add.at(N, F[:, k], fn)
    return N/np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
def orient(V, F):
    vol = np.einsum('ij,ij->i', V[F[:, 0]], np.cross(V[F[:, 1]], V[F[:, 2]])).sum(); return F if vol > 0 else F[:, ::-1]
def write_glb(path, parts, materials):
    mat_names = [m[0] for m in materials]
    deltas = [{m: morph_delta(m, p.V, p.tag) for m in MORPHS} for p in parts]
    active = [m for m in MORPHS if any(np.abs(d[m]).max() > 1e-6 for d in deltas)]
    buf = bytearray(); views, accs = [], []
    def add(arr, comp, typ, target=None, minmax=False):
        while len(buf) % 4: buf.append(0)
        off = len(buf); b = arr.tobytes(); buf.extend(b); v = {"buffer": 0, "byteOffset": off, "byteLength": len(b)}
        if target: v["target"] = target
        views.append(v); a = {"bufferView": len(views)-1, "componentType": comp, "count": len(arr), "type": typ}
        if minmax: a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accs.append(a); return len(accs)-1
    prims = []
    for p, dl in zip(parts, deltas):
        F = orient(p.V, p.F); J, W = skin(p)
        uv = np.stack([(np.arctan2(p.V[:, 2], p.V[:, 0])/(2*np.pi)) % 1, 1-p.V[:, 1]/2], 1)
        pr = {"attributes": {"POSITION": add(p.V.astype(np.float32), 5126, "VEC3", 34962, True),
                             "NORMAL": add(normals(p.V, F).astype(np.float32), 5126, "VEC3", 34962),
                             "TEXCOORD_0": add(uv.astype(np.float32), 5126, "VEC2", 34962),
                             "JOINTS_0": add(J.astype(np.uint16), 5123, "VEC4", 34962),
                             "WEIGHTS_0": add(W.astype(np.float32), 5126, "VEC4", 34962)},
              "indices": add(F.astype(np.uint32).reshape(-1), 5125, "SCALAR", 34963), "material": mat_names.index(p.mat)}
        if active: pr["targets"] = [{"POSITION": add(dl[m].astype(np.float32), 5126, "VEC3", 34962, True)} for m in active]
        prims.append(pr)
    ibm = np.stack([np.array([[1, 0, 0, 0], [0, 1, 0, 0], [0, 0, 1, 0], [-x, -y, -z, 1]], np.float32).reshape(-1) for x, y, z in (POS[b[0]] for b in BONES)])
    ibm_acc = add(ibm, 5126, "MAT4")
    nodes = []
    for name, parent, _ in BONES:
        node = {"name": name, "translation": [float(v) for v in POS[name]-(POS[parent] if parent else 0)]}
        kids = [BI[b[0]] for b in BONES if b[1] == name]
        if kids: node["children"] = kids
        nodes.append(node)
    mname = path.split('/')[-1][:-4]; nodes.append({"name": mname, "mesh": 0, "skin": 0})
    mesh = {"name": mname, "primitives": prims}
    if active: mesh["weights"] = [0.0]*len(active); mesh["extras"] = {"targetNames": active}
    g = {"asset": {"version": "2.0", "generator": "PVV character kit v2"}, "scene": 0, "scenes": [{"nodes": [0, len(nodes)-1]}],
         "nodes": nodes, "meshes": [mesh],
         "skins": [{"name": "SK_PVV", "joints": list(range(len(BONES))), "inverseBindMatrices": ibm_acc, "skeleton": 0}],
         "materials": [{"name": n, "doubleSided": ds, "pbrMetallicRoughness": {"baseColorFactor": list(c)+[1.0], "metallicFactor": met, "roughnessFactor": ro}} for n, c, met, ro, ds in materials],
         "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}]}
    while len(buf) % 4: buf.append(0)
    js = json.dumps(g, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    open(path, 'wb').write(struct.pack('<III', 0x46546C67, 2, 28+len(js)+len(buf))+struct.pack('<II', len(js), 0x4E4F534A)+js+struct.pack('<II', len(buf), 0x004E4942)+bytes(buf))
    return active
def M(name, rgb, met=0., ro=.8, ds=True): return (name, rgb, met, ro, ds)
