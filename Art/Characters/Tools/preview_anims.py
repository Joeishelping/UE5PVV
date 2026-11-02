"""Contact sheet of the animation pack (2 frames per animation, front + side)."""
import sys, numpy as np
sys.path.insert(0, ".")
from PIL import Image, ImageDraw
from preview_heads import load as load_acc, draw
from add_face_morphs import load
import build_anims as BA

def qmat(q):
    x, y, z, w = q
    return np.array([[1-2*(y*y+z*z), 2*(x*y-z*w), 2*(x*z+y*w)], [2*(x*y+z*w), 1-2*(x*x+z*z), 2*(y*z-x*w)], [2*(x*z-y*w), 2*(y*z+x*w), 1-2*(x*x+y*y)]])

g, acc = load_acc(BA.BODY)
nodes = g["nodes"]; joints = g["skins"][0]["joints"]
parent = {c: i for i, n in enumerate(nodes) for c in n.get("children", [])}
def rest_world(i):
    t = np.array(nodes[i].get("translation", [0, 0, 0]))
    return t + (rest_world(parent[i]) if i in parent else 0)
REST = {i: rest_world(i) for i in range(len(nodes))}
prims = []
for prim in g["meshes"][0]["primitives"]:
    V = acc(prim["attributes"]["POSITION"]).astype(float); J = acc(prim["attributes"]["JOINTS_0"]); W = acc(prim["attributes"]["WEIGHTS_0"])
    F = acc(prim["indices"]).reshape(-1, 3)
    mname = g["materials"][prim["material"]]["name"]
    col = (0.62, 0.42, 0.28) if mname == "M_Skin" else tuple(g["materials"][prim["material"]]["pbrMetallicRoughness"]["baseColorFactor"][:3])
    prims.append((V, J, W, F, col))

def posed_tris(pose):
    M = {}
    def world(i):
        if i in M: return M[i]
        n = nodes[i]; name = n.get("name")
        t = np.array(n.get("translation", [0, 0, 0]), float)
        if name == "pelvis" and "pelvis_T" in pose: t = t + np.array(pose["pelvis_T"])
        R = qmat(pose[name]) if name in pose else np.eye(3)
        if i in parent:
            pR, pT = world(parent[i]); M[i] = (pR @ R, pT + pR @ t)
        else:
            M[i] = (R, t)
        return M[i]
    tris = []
    for V, J, W, F, col in prims:
        out = np.zeros_like(V)
        for k in range(4):
            for jj in np.unique(J[:, k]):
                m = J[:, k] == jj; node = joints[jj]; R, T = world(node)
                out[m] += W[m, k:k+1] * ((V[m] - REST[node]) @ R.T + T)
        for a, b, c in F: tris.append((out[a], out[b], out[c], col))
    return tris

def sheet(fn, cols=6):
    S = 220; n = len(BA.ANIMS); rows = (n + cols - 1) // cols
    img = Image.new("RGB", (S * cols * 2, S * rows), (20, 20, 20))
    for i, (name, f, length, loop) in enumerate(BA.ANIMS):
        p = {}; f(0.25 if loop else length * 0.45, p)
        T = posed_tris(p)
        for j, yaw in enumerate((0, 90)):
            im = draw(T, yaw, S=S, center=(0, .95, 0), scale=1.0)
            ImageDraw.Draw(im).text((4, 4), name, fill=(255, 255, 255))
            img.paste(im, (((i % cols) * 2 + j) * S, (i // cols) * S))
    img.save(fn)

if __name__ == "__main__":
    sheet(sys.argv[1] if len(sys.argv) > 1 else "/tmp/anims.png")
