import json, struct, numpy as np, colorsys
from PIL import Image, ImageDraw
def load(path):
    d = open(path, 'rb').read(); jl = struct.unpack_from('<I', d, 12)[0]; g = json.loads(d[20:20+jl]); b = d[20+jl+8:]
    def acc(i):
        a = g["accessors"][i]; v = g["bufferViews"][a["bufferView"]]
        dt = {5126: np.float32, 5123: np.uint16, 5125: np.uint32}[a["componentType"]]; n = {"SCALAR": 1, "VEC2": 2, "VEC3": 3, "VEC4": 4, "MAT4": 16}[a["type"]]
        return np.frombuffer(b, dt, a["count"]*n, v["byteOffset"]).reshape(a["count"], n)
    return g, acc
def rot(axis, deg):
    a = np.radians(deg); x, y, z = np.array(axis)/np.linalg.norm(axis); c, s = np.cos(a), np.sin(a)
    return np.array([[c+x*x*(1-c), x*y*(1-c)-z*s, x*z*(1-c)+y*s], [y*x*(1-c)+z*s, c+y*y*(1-c), y*z*(1-c)-x*s], [z*x*(1-c)-y*s, z*y*(1-c)+x*s, c+z*z*(1-c)]])
def posed(g, pose):
    nodes = g["nodes"]; skin = g["skins"][0]; names = [nodes[j]["name"] for j in skin["joints"]]
    parent = {}
    for i, n in enumerate(nodes):
        for c in n.get("children", []): parent[c] = i
    rest = {}; M = {}
    def world(i):
        if i in M: return M[i]
        t = np.array(nodes[i].get("translation", [0, 0, 0])); R = pose.get(nodes[i]["name"], np.eye(3))
        if i in parent:
            pR, pT = world(parent[i]); M[i] = (pR @ R, pT + pR @ t)
        else: M[i] = (R, t)
        return M[i]
    restW = {}
    def rworld(i):
        t = np.array(nodes[i].get("translation", [0, 0, 0])); return t + (rworld(parent[i]) if i in parent else 0)
    return [(world(j), rworld(j)) for j in skin["joints"]]
def render(files, fn, pose={}, views=((0, 0), (90, 0), (35, -15))):
    S = 420; sheet = Image.new('RGB', (S*len(views), S), (32, 34, 40))
    tris = []
    for f in files:
        g, acc = load(f); X = posed(g, pose)
        for prim in g["meshes"][0]["primitives"]:
            V = acc(prim["attributes"]["POSITION"]).astype(float); J = acc(prim["attributes"]["JOINTS_0"]); W = acc(prim["attributes"]["WEIGHTS_0"])
            out = np.zeros_like(V)
            for k in range(4):
                for jj in np.unique(J[:, k]):
                    m = J[:, k] == jj; (R, T), rest = X[jj]
                    out[m] += W[m, k:k+1] * ((V[m] - rest) @ R.T + T)
            col = g["materials"][prim["material"]]["pbrMetallicRoughness"]["baseColorFactor"][:3]
            F = acc(prim["indices"]).reshape(-1, 3)
            for a, b, c in F: tris.append((out[a], out[b], out[c], col))
    for vi, (yaw, pitch) in enumerate(views):
        R = rot((1, 0, 0), pitch) @ rot((0, 1, 0), yaw)
        img = Image.new('RGB', (S, S), (32, 34, 40)); d = ImageDraw.Draw(img); items = []
        L = np.array([.4, .7, .6]); L /= np.linalg.norm(L)
        for a, b, c, col in tris:
            pa, pb, pc = R @ a, R @ b, R @ c; n = np.cross(pb - pa, pc - pa); ln = np.linalg.norm(n)
            if ln == 0 or n[2] <= 0: continue
            sh = .35 + .65 * max(0, (n/ln) @ L)
            items.append(((pa[2]+pb[2]+pc[2])/3, (pa, pb, pc), tuple(int(255*min(1, x*sh)) for x in col)))
        sc = S/2.2
        for _, pts, col in sorted(items, key=lambda x: x[0]):
            d.polygon([(S/2 + p[0]*sc, S - 15 - p[1]*sc) for p in pts], fill=col)
        sheet.paste(img, (vi*S, 0))
    sheet.save(fn)
import sys
render(["SK_PVV_Body.glb"], "prev_body.png")
pose = {"thigh_l": rot((1, 0, 0), -50), "calf_l": rot((1, 0, 0), 60), "upperarm_r": rot((0, 0, 1), -70), "lowerarm_r": rot((0, 0, 1), -40),
        "spine_02": rot((0, 0, 1), 10), "head": rot((0, 1, 0), 25), "thigh_r": rot((1, 0, 0), 20)}
render(["SK_PVV_Body.glb"], "prev_pose.png", pose, views=((0, 0), (70, 0)))
render(["SK_PVV_Body.glb", "SK_PVV_VikingHelmet.glb", "SK_PVV_Beard.glb"], "prev_viking.png", views=((0, 0), (35, -10)))
render(["SK_PVV_Body.glb", "SK_PVV_PirateHat.glb", "SK_PVV_Eyepatch.glb"], "prev_pirate.png", views=((0, 0), (35, -10)))
render(["SK_PVV_Body.glb", "SK_PVV_Bandana.glb"], "prev_bandana.png", views=((0, 0), (160, -10)))
