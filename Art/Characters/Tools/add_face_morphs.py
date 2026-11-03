"""Adds face shape keys (morph targets) to the existing skinned .glb files.

Body:            Head_Big, Head_Small, Nose_Big, Nose_Small, Lips_Big, Lips_Small, Ears_Big, Ears_Small, Blink
Head-worn items: Head_Big, Head_Small (hair, hats, glasses grow/shrink with the head)

The game sets these from the menu sliders (0 = small, 0.5 = normal, 1 = big).
Run from this folder:  python add_face_morphs.py
"""
import json, struct, numpy as np

HEAD_C = np.array([0., 1.52, 0.])      # centre of the head sphere (glTF: Y up, +Z = face)
HEAD_R = .285
HEAD_PIVOT = np.array([0., 1.30, 0.])  # chin/neck: the head grows from here so it stays on the neck
NOSE_BASE = np.array([0., 1.50, .275])
NOSE_TIP = np.array([0., 1.505, .31])
LIP_C = np.array([0., 1.405, .245])
EYE_CY = 1.5995                        # height of the middle of the eyes
EAR_ROOT = {1: np.array([.30, 1.535, .035]), -1: np.array([-.30, 1.535, .035])}

CT = {5126: np.float32, 5123: np.uint16, 5125: np.uint32, 5121: np.uint8}
NC = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}


def load(path):
    d = open(path, 'rb').read()
    n = struct.unpack('<I', d[12:16])[0]
    g = json.loads(d[20:20 + n])
    o = 20 + n
    bl = struct.unpack('<I', d[o:o + 4])[0]
    return g, bytearray(d[o + 8:o + 8 + bl])


def save(path, g, buf):
    while len(buf) % 4:
        buf.append(0)
    g["buffers"] = [{"byteLength": len(buf)}]
    js = json.dumps(g, separators=(',', ':')).encode()
    while len(js) % 4:
        js += b' '
    open(path, 'wb').write(struct.pack('<III', 0x46546C67, 2, 28 + len(js) + len(buf)) + struct.pack('<II', len(js), 0x4E4F534A) + js
                           + struct.pack('<II', len(buf), 0x004E4942) + bytes(buf))


def read(g, buf, i):
    a = g['accessors'][i]
    v = g['bufferViews'][a['bufferView']]
    dt, nc = CT[a['componentType']], NC[a['type']]
    o = v.get('byteOffset', 0) + a.get('byteOffset', 0)
    return np.frombuffer(bytes(buf[o:o + np.dtype(dt).itemsize * nc * a['count']]), dt).reshape(a['count'], nc).astype(float)


def add_accessor(g, buf, arr):
    while len(buf) % 4:
        buf.append(0)
    arr = arr.astype(np.float32)
    off = len(buf)
    buf.extend(arr.tobytes())
    g['bufferViews'].append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes, "target": 34962})
    g['accessors'].append({"bufferView": len(g['bufferViews']) - 1, "componentType": 5126, "count": len(arr), "type": "VEC3",
                           "min": arr.min(0).tolist(), "max": arr.max(0).tolist()})
    return len(g['accessors']) - 1


def smooth(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def head_weight(g, buf, prim):
    names = [g['nodes'][j]['name'] for j in g['skins'][0]['joints']]
    hi = names.index('head')
    J = read(g, buf, prim['attributes']['JOINTS_0'])
    W = read(g, buf, prim['attributes']['WEIGHTS_0'])
    return ((J == hi) * W).sum(1)


def body_deltas(V, hw, mat):
    r = np.linalg.norm(V - HEAD_C, axis=1)
    out = {}
    head = (V - HEAD_PIVOT) * hw[:, None]
    out["Head_Big"] = head * .22
    out["Head_Small"] = head * -.18

    # Nose: the bump on the front of the face, fading out at its base.
    if mat == "M_Skin":
        d = np.linalg.norm((V - NOSE_TIP) * [1, .8, 1.4], axis=1)
        w = (1 - smooth((d - .035) / .04)) * (V[:, 2] > .2) * (hw > .5)
        out["Nose_Big"] = (V - NOSE_BASE) * (.6 * w)[:, None]
        out["Nose_Small"] = (V - NOSE_BASE) * (-.4 * w)[:, None]
    else:
        out["Nose_Big"] = out["Nose_Small"] = np.zeros_like(V)

    # Lips: their own material, scaled around the mouth centre (fuller = taller + pushed out).
    if mat == "M_Lip":
        rel = V - LIP_C
        out["Lips_Big"] = rel * [.35, .6, .5]
        out["Lips_Small"] = rel * [-.3, -.45, 0.] + [0., 0., .006]  # shrink, but stay on the face
    else:
        out["Lips_Big"] = out["Lips_Small"] = np.zeros_like(V)

    # Ears: whatever sticks out past the head sphere on the sides.
    if mat == "M_Skin":
        side = np.sign(V[:, 0])
        root = np.where(side[:, None] > 0, EAR_ROOT[1], EAR_ROOT[-1])
        w = smooth((r - HEAD_R - .002) / .012) * (np.abs(V[:, 0]) > .2) * (V[:, 1] > 1.42) * (V[:, 1] < 1.66) * (hw > .5)
        out["Ears_Big"] = (V - root) * (.6 * w)[:, None]
        out["Ears_Small"] = (V - root) * (-.3 * w)[:, None]
    else:
        out["Ears_Big"] = out["Ears_Small"] = np.zeros_like(V)

    # Blink: squash each eyeball (and pupil) flat onto the middle line of the eye, so it reads as a closed slit.
    if mat in ("M_Eye", "M_Pupil"):
        d = np.zeros_like(V)
        d[:, 1] = (EYE_CY - V[:, 1]) * .93
        if mat == "M_Pupil":
            d[:, 2] = -.006  # tuck the pupil behind the closed lid
        out["Blink"] = d
    else:
        out["Blink"] = np.zeros_like(V)
    return out


def head_item_deltas(V, hw, mat):
    head = (V - HEAD_PIVOT) * hw[:, None]
    return {"Head_Big": head * .22, "Head_Small": head * -.18}


def process(src, dst, fn):
    g, buf = load(src)
    mesh = g['meshes'][0]
    mats = [m['name'] for m in g['materials']]
    names = list(mesh.get('extras', {}).get('targetNames', []))
    per_prim = []
    for p in mesh['primitives']:
        V = read(g, buf, p['attributes']['POSITION'])
        per_prim.append(fn(V, head_weight(g, buf, p), mats[p['material']]))
    new = [n for n in per_prim[0] if n not in names]
    for p, deltas in zip(mesh['primitives'], per_prim):
        targets = p.setdefault('targets', [])
        for n in new:
            targets.append({"POSITION": add_accessor(g, buf, deltas[n])})
    names += new
    mesh['weights'] = [0.0] * len(names)
    mesh.setdefault('extras', {})['targetNames'] = names
    save(dst, g, buf)
    print(dst, '->', names)


if __name__ == "__main__":
    process("../SK_PVV_Body_Custom.glb", "../SK_PVV_Body_Custom.glb", body_deltas)
    if "--body-only" in __import__("sys").argv:
        raise SystemExit
    import glob
    for f in ["../SK_PVV_Hair_01.glb", "../Outfits/SK_Outfit_CowboyHat.glb", "../Outfits/SK_Outfit_PatrolCap.glb",
              "../Outfits/SK_Outfit_Sunglasses.glb"] + sorted(glob.glob("../SK_Hair_*.glb")) + sorted(glob.glob("../SK_Face_*.glb")) + sorted(glob.glob("../SK_Hat_*.glb")):
        process(f, f, head_item_deltas)
