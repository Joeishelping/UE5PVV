"""Nicer lips on SK_PVV_Body_Custom.glb: smaller, thinner, a crease between upper and lower lip,
a natural lip colour, and a gentler Lips_Big shape key (so maxed-out lips aren't ridiculous).
Run once from this folder:  python fix_lips.py"""
import numpy as np
from add_face_morphs import load, save, read, add_accessor

BODY = "../SK_PVV_Body_Custom.glb"
g, buf = load(BODY)
mesh = g['meshes'][0]
names = mesh['extras']['targetNames']
for p in mesh['primitives']:
    mat = g['materials'][p['material']]
    if mat['name'] != 'M_Lip':
        continue
    V = read(g, buf, p['attributes']['POSITION'])
    C = np.array([0., (V[:, 1].min() + V[:, 1].max()) / 2, V[:, 2].min() + .6 * (V[:, 2].max() - V[:, 2].min())])
    rel = V - C
    W = C + rel * np.array([.82, .66, .8])                     # narrower, thinner, less puffy
    # Crease between the lips (the mouth line), only on the front surface.
    front = np.clip((W[:, 2] - C[2]) / .02, 0, 1)
    W[:, 2] -= .005 * np.exp(-((W[:, 1] - C[1]) / .0045) ** 2) * front
    # Corners tuck back into the cheeks a little (a smile line instead of a flat blob).
    W[:, 2] -= .006 * np.clip((np.abs(W[:, 0]) - .03) / .04, 0, 1) ** 2
    W[:, 1] += .004 * np.clip((np.abs(W[:, 0]) - .03) / .04, 0, 1)
    p['attributes']['POSITION'] = add_accessor(g, buf, W)
    # Normals for the new shape.
    F = read(g, buf, p['indices']).astype(int).reshape(-1, 3)
    N = np.zeros_like(W)
    fn = np.cross(W[F[:, 1]] - W[F[:, 0]], W[F[:, 2]] - W[F[:, 0]])
    for k in range(3):
        np.add.at(N, F[:, k], fn)
    N /= np.maximum(np.linalg.norm(N, axis=1, keepdims=True), 1e-9)
    old_n = read(g, buf, p['attributes']['NORMAL'])
    if (np.einsum('ij,ij->i', N, old_n) < 0).mean() > .5:
        N = -N
    p['attributes']['NORMAL'] = add_accessor(g, buf, N)
    # Shape keys: the lip-size ones are scaled down (big lips = fuller, not a clown mouth).
    for name, k in (('Lips_Big', .4), ('Lips_Small', .6)):
        i = names.index(name)
        d = read(g, buf, p['targets'][i]['POSITION'])
        p['targets'][i]['POSITION'] = add_accessor(g, buf, d * k)
    # Natural lip colour (a muted, slightly darker-than-skin rose instead of bright red).
    mat['pbrMetallicRoughness']['baseColorFactor'] = [0.30, 0.085, 0.075, 1.0]
    mat['pbrMetallicRoughness']['roughnessFactor'] = 0.55
save(BODY, g, buf)
print("lips fixed")
