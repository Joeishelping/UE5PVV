"""Special outfits with skills + the held trumpet.

Hats (head slot, skeletal, follow the body):  SK_Hat_Sombrero, SK_Hat_HardHat, SK_Hat_CannonHelmet
Body:                                          SK_Outfit_Poncho  (disguise)
Held prop (static mesh):                       SM_Trumpet
Run from this folder:  python build_specials.py   then  python add_face_morphs.py
"""
import json, os, struct
import numpy as np
from kit import *

OUT = ".."


def write(name, parts, mats):
    write_glb(os.path.join(OUT, name + ".glb"), parts, mats)
    print("wrote", name)


# ---------------------------------------------------------------- Sombrero: huge brim, tall pointy crown
th = TH(80)
crown_rings = []
for t in np.linspace(0, 1, 12):
    r = (HR + .03) * (1 - .55 * t ** 1.4)
    crown_rings.append(np.stack([r * np.cos(th), np.full_like(th, 1.66 + .26 * t), r * np.sin(th)], 1))
cv, cf = grid(crown_rings, None, (0, 1.93, 0))
brim_rings = []
for t in np.linspace(0, 1, 12):
    r = HR + .03 + .42 * t
    curl = .09 * t ** 3  # edge curls up
    brim_rings.append(np.stack([r * np.cos(th), np.full_like(th, 1.665 + curl), r * np.sin(th)], 1))
bv, bf = grid(brim_rings)
brim = (np.vstack([bv, bv + [0, .012, 0]]), np.vstack([bf, bf[:, ::-1] + len(bv)]))
band = ring_torus(1.69, HR + .035, .02, S=64, s=8, depth=1.0)
dots = merge(*[ellipsoid((np.cos(a) * .55, 1.69 + .06, np.sin(a) * .55), (.03, .015, .03), 8, 4) for a in np.linspace(0, 2 * np.pi, 12, endpoint=False)])
write("SK_Hat_Sombrero", [Part((cv, cf), "hat", "M_Straw"), Part(brim, "hat", "M_Straw"), Part(band, "hat", "M_Band"), Part(dots, "hat", "M_Band")],
      [M("M_Straw", (0.85, 0.72, 0.42)), M("M_Band", (0.75, 0.1, 0.1))])

# ---------------------------------------------------------------- Hard hat: yellow dome + brim + ridge
dome = cap(lambda th, ph: np.full_like(th, HR + .035), lambda th: np.full_like(th, 1.6), S=56)
bth = TH(64)
hb = [np.stack([r * np.cos(bth), np.full(64, 1.6), (r + .06 * (np.sin(bth) > .3)) * np.sin(bth)], 1) for r in (HR + .03, HR + .07)]
hv, hf = grid(hb)
hbrim = (np.vstack([hv, hv + [0, .012, 0]]), np.vstack([hf, hf[:, ::-1] + len(hv)]))
ridge = tube([(0, 1.62, HR + .03), (0, HC[1] + HR + .045, 0), (0, 1.62, -HR - .03)], [.022], S=8, n=10)
write("SK_Hat_HardHat", [Part(dome, "hat", "M_Yellow"), Part(hbrim, "hat", "M_Yellow"), Part(ridge, "hat", "M_Yellow")],
      [M("M_Yellow", (1.0, 0.78, 0.05), 0., .35)])

# ---------------------------------------------------------------- Cannonball helmet: silver dome, red stripe, goggles
helm = cap(lambda th, ph: np.full_like(th, HR + .04), lambda th: hairline_std(th, 1.62, 1.42), S=56)
stripe = tube([(0, 1.6, HR + .045), (0, HC[1] + HR + .05, 0), (0, 1.45, -HR - .045)], [.03], S=8, n=12)
goggles = merge(ellipsoid((.085, 1.66, HR + .015), (.055, .04, .02), 14, 6), ellipsoid((-.085, 1.66, HR + .015), (.055, .04, .02), 14, 6))
strap = ring_torus(1.66, HR + .045, .012, S=56, s=6, depth=1.0)
write("SK_Hat_CannonHelmet", [Part(helm, "hat", "M_Silver"), Part(stripe, "hat", "M_Red"), Part(goggles, "hat", "M_Lens"), Part(strap, "hat", "M_Strap")],
      [M("M_Silver", (0.8, 0.82, 0.85), .9, .25), M("M_Red", (0.85, 0.08, 0.06)), M("M_Lens", (0.1, 0.4, 0.6), .6, .1), M("M_Strap", (0.1, 0.08, 0.06))])

# ---------------------------------------------------------------- Lucha mask: the real head shape, puffed out a little
def _load_body(path="../SK_PVV_Body_Custom.glb"):
    d = open(path, "rb").read(); jl = struct.unpack_from("<I", d, 12)[0]; g = json.loads(d[20:20 + jl]); b = d[20 + jl + 8:]
    def acc(i):
        a = g["accessors"][i]; v = g["bufferViews"][a["bufferView"]]
        n = {"SCALAR": 1, "VEC3": 3}[a["type"]]; dt = {5126: np.float32, 5125: np.uint32, 5123: np.uint16}[a["componentType"]]
        return np.frombuffer(b, dt, a["count"] * n, v.get("byteOffset", 0) + a.get("byteOffset", 0)).reshape(-1, n) if n > 1 else \
            np.frombuffer(b, dt, a["count"], v.get("byteOffset", 0) + a.get("byteOffset", 0))
    for prim in g["meshes"][0]["primitives"]:
        if g["materials"][prim["material"]]["name"] == "M_Skin":
            return acc(prim["attributes"]["POSITION"]).astype(float), acc(prim["indices"]).reshape(-1, 3).astype(int)
    raise RuntimeError("no M_Skin in body")

BV, BF = _load_body()
cent = BV[BF].mean(1)
keep = cent[:, 1] > 1.30
EYE_X, EYE_Y = .105, 1.60
def _in_ellipse(c, cx, cy, rx, ry): return ((c[:, 0] - cx) / rx) ** 2 + ((c[:, 1] - cy) / ry) ** 2
front = cent[:, 2] > .12
eye_d = np.minimum(_in_ellipse(cent, EYE_X, EYE_Y, .062, .045), _in_ellipse(cent, -EYE_X, EYE_Y, .062, .045))
mouth_d = _in_ellipse(cent, 0., 1.405, .085, .05)
hole = front & ((eye_d < 1) | (mouth_d < 1))
keep &= ~hole
trim = front & (((eye_d >= 1) & (eye_d < 2.1)) | ((mouth_d >= 1) & (mouth_d < 1.9)))
crest = (np.abs(cent[:, 0]) < .03) & (cent[:, 1] > 1.66)
flame = front & (cent[:, 1] > 1.66) & (cent[:, 1] < 1.76) & (np.abs(np.abs(cent[:, 0]) - (.06 + (cent[:, 1] - 1.66) * 1.3)) < .018)
Nrm = normals(BV, BF)
PV = BV + Nrm * .009  # puff it out so it sits on top of the skin

def _sub(mask):
    F = BF[mask]; used = np.unique(F); remap = -np.ones(len(PV), int); remap[used] = np.arange(len(used))
    return PV[used], remap[F]
red = keep & ~trim & ~crest & ~flame
write("SK_Hat_LuchaMask", [Part(_sub(red), "hat", "M_MaskRed"), Part(_sub(keep & trim), "hat", "M_MaskWhite"),
                           Part(_sub(keep & (crest | flame) & ~trim), "hat", "M_MaskGold")],
      [M("M_MaskRed", (0.75, 0.05, 0.12), 0., .4), M("M_MaskWhite", (0.95, 0.95, 0.95)), M("M_MaskGold", (1.0, 0.78, 0.15), .6, .3)])

# ---------------------------------------------------------------- Poncho: wide striped cone over the torso
pth = TH(56)
prings = []
for t in np.linspace(0, 1, 16):
    y = 1.32 - .62 * t
    r = float(env_r(np.array([y]))[0]) + .05 + .12 * t + .06 * (1 - t)
    prings.append(np.stack([r * np.cos(pth), np.full(56, y), r * np.sin(pth) * .92], 1))
pv, pf = grid(prings)
stripes = []
for y in (1.05, .92, .80):
    t = (1.32 - y) / .62
    stripes.append(ring_torus(y, float(env_r(np.array([y]))[0]) + .05 + .12 * t + .06 * (1 - t) + .008, .012, S=56, s=6, depth=.92))
write("SK_Outfit_Poncho", [Part((pv, pf), "shawl", "M_Wool"), Part(merge(*stripes), "shawl", "M_Stripe")],
      [M("M_Wool", (0.75, 0.25, 0.15)), M("M_Stripe", (0.95, 0.8, 0.2))])


# ---------------------------------------------------------------- Trumpet (static mesh, held in the right hand)
def write_static(path, parts, mats):
    """Plain (unskinned) glb: parts = [(V, F, material_name)]. Units: metres, +X = forward along the horn."""
    buf = bytearray(); views, accs = [], []

    def add(arr, comp, typ, target=None, minmax=False):
        while len(buf) % 4:
            buf.append(0)
        off = len(buf); b = arr.tobytes(); buf.extend(b)
        v = {"buffer": 0, "byteOffset": off, "byteLength": len(b)}
        if target: v["target"] = target
        views.append(v)
        a = {"bufferView": len(views) - 1, "componentType": comp, "count": len(arr), "type": typ}
        if minmax: a["min"] = arr.min(0).tolist(); a["max"] = arr.max(0).tolist()
        accs.append(a); return len(accs) - 1
    names = [m[0] for m in mats]
    prims = []
    for V, F, mat in parts:
        F = orient(V, F)
        prims.append({"attributes": {"POSITION": add(V.astype(np.float32), 5126, "VEC3", 34962, True),
                                     "NORMAL": add(normals(V, F).astype(np.float32), 5126, "VEC3", 34962)},
                      "indices": add(F.astype(np.uint32).reshape(-1), 5125, "SCALAR", 34963), "material": names.index(mat)})
    name = os.path.basename(path)[:-4]
    g = {"asset": {"version": "2.0", "generator": "PVV specials"}, "scene": 0, "scenes": [{"nodes": [0]}],
         "nodes": [{"name": name, "mesh": 0}], "meshes": [{"name": name, "primitives": prims}],
         "materials": [{"name": n, "pbrMetallicRoughness": {"baseColorFactor": list(c) + [1.0], "metallicFactor": met, "roughnessFactor": ro}} for n, c, met, ro, ds in mats],
         "accessors": accs, "bufferViews": views, "buffers": [{"byteLength": len(buf)}]}
    while len(buf) % 4: buf.append(0)
    js = json.dumps(g, separators=(',', ':')).encode()
    while len(js) % 4: js += b' '
    open(path, 'wb').write(struct.pack('<III', 0x46546C67, 2, 28 + len(js) + len(buf)) + struct.pack('<II', len(js), 0x4E4F534A) + js
                           + struct.pack('<II', len(buf), 0x004E4942) + bytes(buf))
    print("wrote", name)


# glTF: Y up. Horn points along +Z here (forward); Unreal turns it to +X on import.
pipe = tube([(0, 0, -.12), (0, 0, .18)], [.012], S=12, n=2)
bell_rings = []
bth = TH(40)
for t in np.linspace(0, 1, 10):
    r = .014 + .07 * t ** 2.5
    bell_rings.append(np.stack([r * np.cos(bth), r * np.sin(bth), np.full(40, .18 + .1 * t)], 1))
bell = grid(bell_rings)
loop = tube([(0, 0, -.06), (0, -.05, -.03), (0, -.05, .06), (0, 0, .1)], [.01], S=8, n=6)
valves = merge(*[tube([(0, .01, z), (0, .06, z)], [.009], S=8, n=1) for z in (-.02, .01, .04)])
mouth = tube([(0, 0, -.16), (0, 0, -.12)], [.016], S=10, n=1)
write_static(os.path.join(OUT, "SM_Trumpet.glb"),
             [(pipe[0], pipe[1], "M_Brass"), (bell[0], bell[1], "M_Brass"), (loop[0], loop[1], "M_Brass"),
              (valves[0], valves[1], "M_Brass"), (mouth[0], mouth[1], "M_Brass")],
             [M("M_Brass", (0.95, 0.75, 0.25), .9, .25)])
