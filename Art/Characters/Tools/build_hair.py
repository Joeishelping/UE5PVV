"""Hairstyles for the custom body (same head sphere as kit.HC / kit.HR). All skinned to the head.

Hair:        Buzz, Fade, SlickBack, CurlyTop, Mullet, LongWavy, HighAndTight, CrewCut, FlatTop, SidePart,
             Afro, Waves, Twists, Cornrows, Pompadour
Facial hair: Mustache, Handlebar, Horseshoe, Pencil, Goatee, FullBeard, Stubble, ChinStrap
Every style works for both teams - put whichever you like in BP_Runner / BP_Warden.

Run from this folder:  python build_hair.py   then  python add_face_morphs.py
"""
import os
import numpy as np
from kit import *

OUT = ".."
HAIR_MAT = [M("M_Hair", (0.06, 0.04, 0.03), 0., .55)]


def ss(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def top(ph, start=.75, width=.35):
    """0 on the sides, 1 on top of the head (ph = latitude, pi/2 = crown)."""
    return ss((ph - start) / width)


def front(th):
    """1 at the forehead, 0 at the back."""
    return (np.sin(th) + 1) / 2


def shell(thick, hairline, S=64, rings=18):
    return cap(lambda th, ph: HR + thick(th, ph), hairline, S=S, rings=rings)


def write_hair(name, parts):
    path = os.path.join(OUT, name + ".glb")
    write_glb(path, [Part(vf, "hair", "M_Hair") for vf in parts], HAIR_MAT)
    print("wrote", path)


std = lambda th: hairline_std(th)
# Low sides so short cuts cover the temples (ears still show).
low = lambda th: hairline_std(th, 1.68, 1.40)

# ------------------------------------------------------------------ shared
write_hair("SK_Hair_Buzz", [shell(lambda th, ph: np.full_like(th, .008), low)])

# ------------------------------------------------------------------ more cuts
write_hair("SK_Hair_Fade", [shell(lambda th, ph: .004 + .04 * top(ph, .8, .3) * (.6 + .4 * front(th)), low)])

write_hair("SK_Hair_SlickBack", [shell(
    lambda th, ph: .022 + .022 * top(ph, .6, .5) + .015 * (1 - front(th)) * ss((1.1 - ph) / .6)
    + .003 * np.sin(th * 26) * top(ph, .5, .4), std)])

curl = lambda th, ph: .012 * np.abs(np.sin(th * 9) * np.sin(ph * 14))
write_hair("SK_Hair_CurlyTop", [shell(lambda th, ph: .005 + (.055 + curl(th, ph)) * top(ph, .75, .3), low, S=72, rings=22)])


def mullet_line(th):
    # Short at the front, long party in the back down to the neck.
    return 1.27 + (1.66 - 1.27) * front(th) ** .7


write_hair("SK_Hair_Mullet", [shell(
    lambda th, ph: .02 + .015 * top(ph) + .05 * (1 - front(th)) ** 2 * ss((.2 - ph) / .6), mullet_line, rings=22)])


def long_line(th):
    return 1.69 * front(th) ** 3 + 1.24 * (1 - front(th) ** 3)


write_hair("SK_Hair_LongWavy", [shell(
    lambda th, ph: .025 + .07 * ss((.4 - ph) / .9) * (1 - .6 * front(th)) + .008 * np.sin(th * 14 + ph * 6),
    long_line, S=72, rings=24)])

# ------------------------------------------------------------------ regulation cuts
write_hair("SK_Hair_HighAndTight", [shell(lambda th, ph: .003 + .025 * top(ph, 1.0, .2), low)])

write_hair("SK_Hair_CrewCut", [shell(lambda th, ph: .008 + .02 * top(ph, .7, .4) * (.7 + .3 * front(th)), low)])

flat_v, flat_f = shell(lambda th, ph: .006 + .07 * top(ph, .85, .25), low)
roof = HC[1] + HR + .035
flat_v[:, 1] = np.minimum(flat_v[:, 1], roof)  # sanded flat on top
write_hair("SK_Hair_FlatTop", [(flat_v, flat_f)])


def side_part(th, ph):
    x = np.cos(ph) * np.cos(th) * HR
    groove = .014 * np.exp(-((x - .09) / .012) ** 2) * top(ph, .5, .3)
    sweep = .012 * ss(-x / .2) * top(ph, .5, .5)  # combed over to one side
    return .02 + .012 * top(ph, .6, .5) + sweep - groove


write_hair("SK_Hair_SidePart", [shell(side_part, std, S=80)])


# ------------------------------------------------------------------ volume styles
afro_bump = lambda th, ph: .01 * np.sin(th * 13) * np.sin(ph * 11)
write_hair("SK_Hair_Afro", [shell(lambda th, ph: .06 + .07 * ss((ph + .2) / 1.2) + afro_bump(th, ph), std, S=72, rings=22)])

waves = lambda th, ph: .0025 * np.sin(np.arccos(np.clip(np.cos(ph) * np.sin(th), -1, 1)) * 40)  # rings around the crown
write_hair("SK_Hair_Waves", [shell(lambda th, ph: .008 + waves(th, ph), low, S=96, rings=26)])

write_hair("SK_Hair_Pompadour", [shell(
    lambda th, ph: .006 + .03 * top(ph, .7, .4) + .06 * top(ph, .8, .4) * ss((np.sin(th) - .2) / .6), low, S=72, rings=22)])


def on_head(th, ph, r):
    return np.array([r * np.cos(ph) * np.cos(th), HC[1] + r * np.sin(ph), r * np.cos(ph) * np.sin(th)])


# Twists: short buzz + little upright twists all over the top.
tw = [shell(lambda th, ph: np.full_like(th, .006), low)]
for ph in np.linspace(.75, 1.45, 5):
    n = max(3, int(14 * np.cos(ph)))
    for th in np.linspace(0, 2 * np.pi, n, endpoint=False) + ph * 3:
        base = on_head(th, ph, HR)
        up = (base - HC) / np.linalg.norm(base - HC)
        tw.append(tube([base - up * .01, base + up * .045, base + up * .085], [.016, .014, .009], S=8, n=3))
write_hair("SK_Hair_Twists", tw)

# Cornrows: braided rows running front to back.
rows = [shell(lambda th, ph: np.full_like(th, .004), low)]
for x in np.linspace(-.2, .2, 7):
    pts = []
    for zf in np.linspace(.75, -.85, 14):
        z = zf * HR
        y2 = HR ** 2 - x ** 2 - z ** 2
        if y2 <= 0:
            continue
        pts.append(HC + [x, np.sqrt(y2) + .006, z])
    if len(pts) > 3:
        rows.append(tube(pts, [.016], S=8, n=3, mod=lambda t: 1 + .25 * np.sin(t * 60)))
write_hair("SK_Hair_Cornrows", rows)

# ------------------------------------------------------------------ facial hair
def face_pt(x, y, off):
    z2 = (HR + off) ** 2 - x ** 2 - (y - HC[1]) ** 2
    return np.array([x, y, np.sqrt(max(z2, 1e-6))])


def mustache_path(width, droop=0., curl=0., y=1.458):
    xs = np.linspace(-width, width, 9)
    pts = []
    for x in xs:
        k = abs(x) / width
        yy = y - .01 * (1 - k) + droop * k ** 2 + curl * max(0, k - .75) * 8
        pts.append(face_pt(x, yy, .028 - .012 * k))  # sits in front of the upper lip
    return pts


def write_face(name, parts):
    path = os.path.join(OUT, name + ".glb")
    write_glb(path, [Part(vf, "facehair", "M_Hair") for vf in parts], HAIR_MAT)
    print("wrote", path)


thick_mid = lambda t: .55 + .45 * np.sin(t * np.pi)
write_face("SK_Face_Mustache", [tube(mustache_path(.075, -.012), [.016], S=10, n=4, mod=thick_mid)])
write_face("SK_Face_Pencil", [tube(mustache_path(.06, -.004), [.006], S=8, n=4, mod=thick_mid)])
write_face("SK_Face_Handlebar", [tube(mustache_path(.11, .0, .02), [.014], S=10, n=4, mod=thick_mid)])
horse = [tube(mustache_path(.08, -.01), [.016], S=10, n=4, mod=thick_mid)]
for sx in (-1, 1):
    horse.append(tube([face_pt(sx * .082, y, .012) for y in np.linspace(1.445, 1.33, 6)], [.014], S=8, n=3))
write_face("SK_Face_Horseshoe", horse)


def beard_sheet(top_y, bottom_y, half_angle, thick, cols=36, rows=12, lip_gap=True):
    """Sheet over the lower face. top_y(a) / bottom_y(a) by angle from the face centre (a = 0 front)."""
    A = np.linspace(-half_angle, half_angle, cols)
    P = []
    for t in np.linspace(0, 1, rows):
        row = []
        for a in A:
            y = bottom_y(a) + (top_y(a) - bottom_y(a)) * t
            th = np.pi / 2 + a
            r = HR + thick(a, t)
            rr = np.sqrt(max(r ** 2 - (y - HC[1]) ** 2, (r * .35) ** 2))
            row.append([rr * np.cos(th), y, rr * np.sin(th)])
        P.append(row)
    V, F = sheet(np.array(P))
    return V, F


def jaw_top(a, mouth=1.352, cheek=1.50):
    # Stays under the lips in the middle, climbs the cheeks to the sideburns.
    return mouth + (cheek - mouth) * ss((abs(a) - .3) / .5)


full_bottom = lambda a: 1.235 + .03 * ss(abs(a) / 1.1)
write_face("SK_Face_FullBeard", [
    beard_sheet(jaw_top, full_bottom, 1.2, lambda a, t: .03 + .02 * (1 - t) + .004 * np.sin(a * 30)),
    tube(mustache_path(.085, -.015), [.017], S=10, n=4, mod=thick_mid)])
write_face("SK_Face_Stubble", [beard_sheet(jaw_top, full_bottom, 1.2, lambda a, t: np.full_like(np.asarray(a, float), .005)),
                               tube(mustache_path(.075, -.012), [.006], S=8, n=4)])
write_face("SK_Face_Goatee", [
    beard_sheet(lambda a: 1.352, lambda a: 1.26 + .02 * abs(a) / .35, .35, lambda a, t: .022 + .01 * (1 - t)),
    tube(mustache_path(.07, -.03), [.014], S=10, n=4, mod=thick_mid)])
write_face("SK_Face_ChinStrap", [beard_sheet(lambda a: full_bottom(a) + .05 + .4 * ss((abs(a) - .6) / .6) * .25,
                                             full_bottom, 1.2, lambda a, t: np.full_like(np.asarray(a, float), .012))])
