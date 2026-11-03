"""More face variety: eyebrows (real geometry) + new shape keys on the body, beards, hair and hats.

Body adds:
  Face_Thin / Face_Wide       cheeks + face width
  Jaw_Soft / Jaw_Strong       jaw line
  Nose_Narrow / Nose_Wide     nose width (wide = flared nostrils)
  Nose_Flat / Nose_Hooked     bridge: flat + broad  <->  high, curved (aquiline / Aztec)
  Nose_Pointy / Nose_Round    tip: small + pointed  <->  round, bulbous
  Eyes_Small / Eyes_Big
  Brow_Thin / Brow_Thick      eyebrow thickness
  Brow_Low / Brow_High        eyebrow height
  Brow_Sad / Brow_Angry       eyebrow angle
Head-worn items (hair, beards, hats) get Face_*/Jaw_* so beards and masks follow the face.

Run from this folder (after add_face_morphs.py):  python add_face_variety.py
"""
import glob
import numpy as np
from add_face_morphs import load, save, read, add_accessor, smooth, head_weight, HEAD_C, HEAD_R, HEAD_PIVOT, NOSE_BASE, NOSE_TIP, EYE_CY

BODY = "../SK_PVV_Body_Custom.glb"
BROW_COLOR = [0.03, 0.018, 0.01, 1.0]
EYE_C = {1: np.array([.1255, EYE_CY, .222]), -1: np.array([-.1255, EYE_CY, .222])}


# ---------------------------------------------------------------------------------------------
# Eyebrow geometry: an arched, tapered strip on the head sphere above each eye.

def brow_strip(side, segs=12):
    """Returns (positions, normals, triangles, t-along, thickness-dir) for one eyebrow."""
    P, N, U, T, F = [], [], [], [], []
    R = HEAD_R
    for i in range(segs + 1):
        t = i / segs
        x = side * (.058 + .122 * t)
        yc = 1.662 + .014 * np.sin(np.pi * (.15 + .75 * t)) - .004 * t   # arch, peaking a bit past the middle
        h = .0125 * (1. - .55 * t) + .002                                 # thick at the inner end, tapering out
        for dy in (h * .5, -h * .5):                                     # top, bottom
            y = yc + dy
            q = np.array([x, y - HEAD_C[1], 0.])
            q[2] = np.sqrt(max(R * R - q[0] ** 2 - q[1] ** 2, 1e-6))
            n = q / np.linalg.norm(q)
            for out in (.0045, -.001):                                   # front skin, buried back
                P.append(HEAD_C + n * (R + out))
                N.append(n)
                U.append((t, 0.5 + dy / max(h, 1e-6)))
                T.append((t, np.sign(dy)))
    # vertex index: seg i, top/bottom k (0/1), front/back f (0/1)
    def vi(i, k, f):
        return i * 4 + k * 2 + f
    for i in range(segs):
        a, b = i, i + 1
        quads = [
            (vi(a, 0, 0), vi(a, 1, 0), vi(b, 1, 0), vi(b, 0, 0)),  # front
            (vi(a, 0, 1), vi(a, 0, 0), vi(b, 0, 0), vi(b, 0, 1)),  # top
            (vi(a, 1, 0), vi(a, 1, 1), vi(b, 1, 1), vi(b, 1, 0)),  # bottom
        ]
        for q in quads:
            if side < 0:
                q = q[::-1]
            F += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
    for i, flip in ((0, side > 0), (segs, side < 0)):  # end caps
        q = (vi(i, 0, 0), vi(i, 0, 1), vi(i, 1, 1), vi(i, 1, 0))
        if flip:
            q = q[::-1]
        F += [(q[0], q[1], q[2]), (q[0], q[2], q[3])]
    return np.array(P), np.array(N), np.array(U), np.array(T), np.array(F)


def add_brows(g, buf):
    mats = [m['name'] for m in g['materials']]
    if "M_Brow" in mats:
        return False
    g['materials'].append({"name": "M_Brow", "pbrMetallicRoughness": {"baseColorFactor": BROW_COLOR, "metallicFactor": 0.0, "roughnessFactor": 0.9}})
    mat = len(g['materials']) - 1
    P, N, U, F = [], [], [], []
    base = 0
    for side in (1, -1):
        p, n, u, t, f = brow_strip(side)
        P.append(p); N.append(n); U.append(u); F.append(f + base)
        base += len(p)
    P = np.concatenate(P); N = np.concatenate(N); U = np.concatenate(U); F = np.concatenate(F).astype(np.uint32)
    joints = [g['nodes'][j]['name'] for j in g['skins'][0]['joints']]
    head = joints.index('head')

    def raw(arr, ctype, typ, target=34962):
        while len(buf) % 4:
            buf.append(0)
        off = len(buf)
        buf.extend(arr.tobytes())
        g['bufferViews'].append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes, **({"target": target} if target else {})})
        acc = {"bufferView": len(g['bufferViews']) - 1, "componentType": ctype, "count": len(arr), "type": typ}
        if typ == "VEC3" and ctype == 5126:
            acc["min"] = arr.min(0).tolist(); acc["max"] = arr.max(0).tolist()
        g['accessors'].append(acc)
        return len(g['accessors']) - 1

    J = np.zeros((len(P), 4), np.uint16); J[:, 0] = head
    W = np.zeros((len(P), 4), np.float32); W[:, 0] = 1.
    prim = {
        "attributes": {
            "POSITION": raw(P.astype(np.float32), 5126, "VEC3"),
            "NORMAL": raw(N.astype(np.float32), 5126, "VEC3"),
            "TEXCOORD_0": raw(U.astype(np.float32), 5126, "VEC2"),
            "JOINTS_0": raw(J, 5123, "VEC4"),
            "WEIGHTS_0": raw(W, 5126, "VEC4"),
        },
        "indices": raw(F.reshape(-1), 5125, "SCALAR", 34963),
        "material": mat,
    }
    # Every primitive needs the same shape keys: the brows follow head size and blink not at all.
    names = g['meshes'][0].get('extras', {}).get('targetNames', [])
    targets = []
    for nme in names:
        d = np.zeros_like(P)
        if nme == "Head_Big":
            d = (P - HEAD_PIVOT) * .22
        elif nme == "Head_Small":
            d = (P - HEAD_PIVOT) * -.18
        targets.append({"POSITION": add_accessor(g, buf, d)})
    prim["targets"] = targets
    g['meshes'][0]['primitives'].append(prim)
    return True


# ---------------------------------------------------------------------------------------------
# Shape keys

def face_shape(V, hw):
    """Face width + jaw (also used for beards, hair, hats so they follow the face)."""
    out = {}
    cheeks = hw * smooth((1.585 - V[:, 1]) / .13)            # everything below the eyes
    jaw = hw * smooth((1.455 - V[:, 1]) / .12)               # the bottom of the face
    front = smooth((V[:, 2] + .05) / .15)                    # more at the front than the back of the head
    d = np.zeros_like(V); d[:, 0] = -.22 * V[:, 0] * cheeks * (.6 + .4 * front); out["Face_Thin"] = d
    d = np.zeros_like(V); d[:, 0] = .22 * V[:, 0] * cheeks * (.6 + .4 * front); out["Face_Wide"] = d
    d = np.zeros_like(V); d[:, 0] = .14 * V[:, 0] * jaw; d[:, 1] = -.014 * jaw * front; d[:, 2] = .02 * jaw * front; out["Jaw_Strong"] = d
    d = np.zeros_like(V); d[:, 0] = -.12 * V[:, 0] * jaw; d[:, 1] = .006 * jaw * front; d[:, 2] = -.012 * jaw * front; out["Jaw_Soft"] = d
    return out


def body_deltas(V, hw, mat):
    out = face_shape(V, hw)
    z = np.zeros_like(V)
    # Nose (skin only): the bump on the front of the face.
    if mat == "M_Skin":
        d = np.linalg.norm((V - NOSE_TIP) * [1, .8, 1.4], axis=1)
        w = (1 - smooth((d - .035) / .04)) * (V[:, 2] > .2) * (hw > .5)
        rel = V - NOSE_BASE
        low = smooth((1.5 - V[:, 1]) / .04)                   # nostril area
        upper = smooth((V[:, 1] - 1.49) / .05)                # bridge
        tipd = np.linalg.norm(V - (NOSE_TIP + [0, -.01, -.005]), axis=1)
        tip = (1 - smooth((tipd - .012) / .025)) * w
        proj = np.clip(V[:, 2] - .27, 0, None)                # how far it sticks out of the face
        d = z.copy(); d[:, 0] = rel[:, 0] * (.85 + .6 * low) * w; out["Nose_Wide"] = d
        d = z.copy(); d[:, 0] = rel[:, 0] * -.38 * w; out["Nose_Narrow"] = d
        d = z.copy(); d[:, 2] = -proj * .45 * w; d[:, 0] = rel[:, 0] * .45 * low * w; d[:, 1] = .004 * low * w; out["Nose_Flat"] = d
        d = z.copy(); d[:, 2] = (proj * .55 + .024 * upper * smooth((proj - .005) / .02)) * w; d[:, 1] = -.013 * tip; d[:, 0] = rel[:, 0] * -.15 * w
        out["Nose_Hooked"] = d
        c = NOSE_TIP + [0, -.012, -.012]
        d = (V - c) * (.55 * tip)[:, None]; out["Nose_Round"] = d
        d = z.copy(); d[:, 2] = proj * .35 * tip; d[:, 0] = rel[:, 0] * -.35 * tip; d[:, 1] = .004 * tip; out["Nose_Pointy"] = d
    else:
        for k in ("Nose_Wide", "Nose_Narrow", "Nose_Flat", "Nose_Hooked", "Nose_Round", "Nose_Pointy"):
            out[k] = z.copy()
    # Eyes.
    if mat in ("M_Eye", "M_Pupil"):
        side = np.where(V[:, 0] > 0, 1, -1)
        c = np.where(side[:, None] > 0, EYE_C[1], EYE_C[-1])
        out["Eyes_Big"] = (V - c) * .22
        out["Eyes_Small"] = (V - c) * -.18
    else:
        out["Eyes_Big"] = z.copy(); out["Eyes_Small"] = z.copy()
    # Eyebrows.
    if mat == "M_Brow":
        x = np.abs(V[:, 0]); inner = 1 - smooth((x - .06) / .12)          # 1 at the inner end, 0 outside
        mid_y = 1.662 + .014 * np.sin(np.pi * (.15 + .75 * np.clip((x - .058) / .122, 0, 1)))
        dy = V[:, 1] - mid_y
        d = z.copy(); d[:, 1] = dy * .9; d[:, 2] = .002; out["Brow_Thick"] = d
        d = z.copy(); d[:, 1] = dy * -.55; out["Brow_Thin"] = d
        d = z.copy(); d[:, 1] = .013; d[:, 2] = -.002; out["Brow_High"] = d
        d = z.copy(); d[:, 1] = -.008; d[:, 2] = .001; out["Brow_Low"] = d
        d = z.copy(); d[:, 1] = -.014 * inner + .004 * (1 - inner); d[:, 2] = .003 * inner; out["Brow_Angry"] = d
        d = z.copy(); d[:, 1] = .013 * inner - .005 * (1 - inner); out["Brow_Sad"] = d
        # Keep the brows sitting ON the head when they move (slide over the sphere, don't sink in).
        r0 = np.linalg.norm(V - HEAD_C, axis=1)
        for k in ("Brow_Thick", "Brow_Thin", "Brow_High", "Brow_Low", "Brow_Angry", "Brow_Sad"):
            moved = V + out[k]
            n = (moved - HEAD_C) / np.linalg.norm(moved - HEAD_C, axis=1)[:, None]
            out[k] = HEAD_C + n * r0[:, None] - V
    else:
        for k in ("Brow_Thick", "Brow_Thin", "Brow_High", "Brow_Low", "Brow_Angry", "Brow_Sad"):
            out[k] = z.copy()
    return out


def item_deltas(V, hw, mat):
    return face_shape(V, hw)


def hair_under_hat(V, hw, mat):
    """Hair_UnderHat: squash the hair down INTO the scalp where a hat sits, so nothing pokes through
    (hats hug the head at ~HEAD_R, so the hair has to go just under it). Below the hat line it's left alone."""
    rel = V - HEAD_C
    r = np.linalg.norm(rel, axis=1)
    cap = HEAD_R - .006                                       # just under the scalp: the hat (and head) hide it
    w = smooth((V[:, 1] - 1.47) / .09)                        # fully squashed from ~1.56 up (hat bands start ~1.60)
    target = np.minimum(r, cap)
    scale = np.where(r > 1e-6, target / np.maximum(r, 1e-6), 1.)
    d = rel * (scale - 1.)[:, None] * w[:, None]
    return {"Hair_UnderHat": d}


def zero_under_hat(V, hw, mat):
    return {"Hair_UnderHat": np.zeros_like(V)}


def process(path, fn, extra=None):
    g, buf = load(path)
    if extra:
        extra(g, buf)
    mesh = g['meshes'][0]
    mats = [m['name'] for m in g['materials']]
    names = list(mesh.get('extras', {}).get('targetNames', []))
    per_prim = [fn(read(g, buf, p['attributes']['POSITION']), head_weight(g, buf, p), mats[p['material']]) for p in mesh['primitives']]
    new = [n for n in per_prim[0] if n not in names]
    for p, deltas in zip(mesh['primitives'], per_prim):
        targets = p.setdefault('targets', [])
        for n in new:
            targets.append({"POSITION": add_accessor(g, buf, deltas[n])})
    names += new
    mesh['weights'] = [0.0] * len(names)
    mesh.setdefault('extras', {})['targetNames'] = names
    save(path, g, buf)
    print(path, '->', len(names), 'shape keys', new)


if __name__ == "__main__":
    process(BODY, body_deltas, add_brows)
    for f in ["../SK_PVV_Hair_01.glb"] + sorted(glob.glob("../SK_Hair_*.glb")) + sorted(glob.glob("../SK_Face_*.glb")) + sorted(glob.glob("../SK_Hat_*.glb")) \
            + ["../Outfits/SK_Outfit_CowboyHat.glb", "../Outfits/SK_Outfit_PatrolCap.glb", "../Outfits/SK_Outfit_Sunglasses.glb"]:
        try:
            process(f, item_deltas)
        except Exception as e:  # noqa
            print('skip', f, e)
