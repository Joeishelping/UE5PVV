import json, struct, numpy as np, sys
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
SKIN=(0.62,0.42,0.28)
def tris_of(files, morphs, hair_col=(0.08,0.05,0.03)):
    T=[]
    for f in files:
        g, acc = load(f); names = g["meshes"][0].get("extras", {}).get("targetNames", [])
        for prim in g["meshes"][0]["primitives"]:
            V = acc(prim["attributes"]["POSITION"]).astype(float).copy()
            for k, tg in enumerate(prim.get("targets", [])):
                w = morphs.get(names[k], 0)
                if w: V += w * acc(tg["POSITION"])
            mname = g["materials"][prim["material"]]["name"]
            col = g["materials"][prim["material"]]["pbrMetallicRoughness"]["baseColorFactor"][:3]
            if mname=="M_Skin": col=SKIN
            if mname=="M_Hair": col=hair_col
            if V[:,1].max() < 1.1: continue
            F = acc(prim["indices"]).reshape(-1, 3)
            for a, b, c in F: T.append((V[a], V[b], V[c], col))
    return T
def draw(T, yaw, S=260, center=(0,1.5,0), scale=2.6):
    R = rot((1,0,0),-8) @ rot((0, 1, 0), yaw); img = Image.new('RGB', (S, S), (40, 42, 50)); d = ImageDraw.Draw(img)
    L = np.array([.4, .6, .7]); L /= np.linalg.norm(L); items=[]
    c=np.array(center)
    for a, b, cc, col in T:
        pa, pb, pc = R @ (a-c), R @ (b-c), R @ (cc-c); n = np.cross(pb - pa, pc - pa); ln = np.linalg.norm(n)
        if ln == 0: continue
        nz=n/ln
        if nz[2] <= 0: nz=-nz
        sh = .35 + .65 * max(0, nz @ L)
        items.append(((pa[2]+pb[2]+pc[2])/3, (pa, pb, pc), tuple(int(255*min(1, x*sh)) for x in col)))
    sc = S*scale/2
    for _, pts, col in sorted(items, key=lambda x: x[0]):
        d.polygon([(S/2 + p[0]*sc, S/2 - p[1]*sc) for p in pts], fill=col)
    return img
def sheet(entries, fn, cols=4, yaws=(0,40)):
    S=260; rows=(len(entries)+cols-1)//cols
    out=Image.new('RGB',(S*cols*len(yaws),S*rows+0),(20,20,20))
    for i,(label,files,m) in enumerate(entries):
        T=tris_of(files,m)
        for j,yw in enumerate(yaws):
            im=draw(T,yw); ImageDraw.Draw(im).text((6,6),label,fill=(255,255,255))
            out.paste(im,(((i%cols)*len(yaws)+j)*S,(i//cols)*S))
    out.save(fn)
