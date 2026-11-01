import numpy as np
from kit import *

OUT = "out"
import os; os.makedirs(OUT, exist_ok=True)

def torso_shell(off, y0, y1, n=40, S=44):
    ys = np.linspace(y0, y1, n); rs = env_r(ys) + off
    return revolve_profile(ys, rs, S)
def hips_shell(off, y0=.27, y1=.64, n=20, S=44):
    ys = np.linspace(y0, y1, n); rs = hips_r(ys) + off
    return revolve_profile(ys, rs, S)
def sleeve(s, off, t1, t0=0.0, flare=1.0):
    P = catmull(ARM[s], 8); R = np.interp(np.linspace(0, 1, len(P)), np.linspace(0, 1, len(ARM_R)), ARM_R)
    i0, i1 = int(t0 * (len(P) - 1)), max(int(t1 * (len(P) - 1)), 2)
    radii = R[i0:i1 + 1] + off
    radii[-1] *= flare
    return tube(P[i0:i1 + 1], radii, S=14, n=1, caps=False)
def pant_leg(s, off, t1=1.0):
    P = catmull(LEG[s], 8); R = np.interp(np.linspace(0, 1, len(P)), np.linspace(0, 1, len(LEG_R)), LEG_R)
    i1 = max(int(t1 * (len(P) - 1)), 2)
    return tube(P[:i1 + 1], R[:i1 + 1] + off, S=16, n=1, caps=False)
def belt(y, off=.03, r=.025, color_tag="hips"):
    return ring_torus(y, float(env_r(np.array([y]))[0]) + off, r, S=56, s=8)

FONT = {
 'B': ["11110","10001","10001","11110","10001","10001","11110"],
 'O': ["01110","10001","10001","10001","10001","10001","01110"],
 'R': ["11110","10001","10001","11110","10100","10010","10001"],
 'D': ["11100","10010","10001","10001","10001","10010","11100"],
 'E': ["11111","10000","10000","11110","10000","10000","11111"],
 'P': ["11110","10001","10001","11110","10000","10000","10000"],
 'A': ["01110","10001","10001","11111","10001","10001","10001"],
 'T': ["11111","00100","00100","00100","00100","00100","00100"],
 'L': ["10000","10000","10000","10000","10000","10000","11111"],
}
def back_text(word, y_top, off, px=.016):
    """Pixel letters on the BACK of the torso envelope (reads correctly from behind)."""
    cols = sum(6 for _ in word) - 1
    parts = []; c0 = 0
    for ch in word:
        for row, line in enumerate(FONT[ch]):
            for col, bit in enumerate(line):
                if bit != '1': continue
                c = c0 + col; y = y_top - row * px
                r = float(env_r(np.array([y]))[0]) + off
                th_c = -np.pi / 2 - (c - cols / 2) * px / r
                dth = px / r * .5; dy = px * .5
                P = np.array([[surface_pt(th_c + a, y + b, off) for a in (-dth, dth)] for b in (dy, -dy)])
                parts.append(sheet(P))
        c0 += 6
    return merge(*parts)

def write(name, parts, mats):
    active = write_glb(f"{OUT}/{name}.glb", parts, mats)
    print(name, "morphs:", len(active))

# ------------------------------------------------------------------ RUNNER SET
write("SK_Outfit_WorkShirt", [
    Part(torso_shell(.014, .52, 1.34), "torso", "M_Shirt"),
    Part(sleeve("l", .012, .55, flare=1.15), "arm_l", "M_Shirt"),
    Part(sleeve("r", .012, .55, flare=1.15), "arm_r", "M_Shirt"),
    Part(torso_shell(.02, 1.14, 1.30, n=8), "torso", "M_ShirtCollar"),
], [M("M_Shirt", (0.55, 0.12, 0.08)), M("M_ShirtCollar", (0.4, 0.08, 0.05))])

write("SK_Outfit_Jeans", [
    Part(hips_shell(.016), "hips", "M_Denim"),
    Part(pant_leg("l", .014), "leg_l", "M_Denim"),
    Part(pant_leg("r", .014), "leg_r", "M_Denim"),
    Part(belt(.62, .035), "hips", "M_Belt"),
    Part(ellipsoid((0, .62, float(hips_r(np.array([.62]))[0]) * DEPTH + .05), (.05, .035, .02), 12, 6), "hips", "M_Buckle"),
], [M("M_Denim", (0.12, 0.2, 0.42)), M("M_Belt", (0.25, 0.14, 0.06)), M("M_Buckle", (0.85, 0.7, 0.25), .8, .35)])

# Cowboy hat: tall crown with a dent + wide brim curled up at the sides.
crown = cap(lambda th, ph: np.full_like(th, HR + .02), lambda th: np.full_like(th, 1.66), S=48)
crown = (crown[0] * [1, 1, 1], crown[1])
cv = crown[0]; top = cv[:, 1] > 1.70
cv[top, 1] = 1.70 + (cv[top, 1] - 1.70) * 1.9  # taller crown
cv[top, 1] -= .03 * np.exp(-(cv[top, 0] / .08) ** 2)  # center dent
th = TH(72); rings = []
for t in np.linspace(0, 1, 10):
    r = .26 + .20 * t
    curl = .12 * t ** 2 * np.abs(np.cos(th)) ** 1.5     # sides curl up
    dip = .03 * t * np.abs(np.sin(th))                   # front/back dip
    rings.append(np.stack([r * np.cos(th), 1.665 + curl - dip, r * np.sin(th)], 1))
bv, bf = grid(rings); bv2 = bv + [0, .012, 0]
brim = (np.vstack([bv, bv2]), np.vstack([bf, bf[:, ::-1] + len(bv)]))
band = ring_torus(1.69, HR + .025, .018, S=56, s=8, depth=1.0)
write("SK_Outfit_CowboyHat", [Part((cv, crown[1]), "hat", "M_Felt"), Part(brim, "hat", "M_Felt"), Part(band, "hat", "M_HatBand")],
      [M("M_Felt", (0.42, 0.28, 0.14)), M("M_HatBand", (0.15, 0.08, 0.04))])

# Neck bandana: loop + a folded triangle hanging in front.
loop = ring_torus(1.29, .17, .035, S=40, s=10, depth=1.0)
tri = merge(*[ellipsoid((0, 1.20 - k * .035, .22 - k * .012), (.13 - k * .028, .03, .03), 16, 8) for k in range(4)])
write("SK_Outfit_Bandana", [Part(loop, "torso", "M_Bandana"), Part(tri, "torso", "M_Bandana")], [M("M_Bandana", (0.75, 0.08, 0.08))])

# ------------------------------------------------------------------ BORDER PATROL SET
green, dark, gold = (0.22, 0.30, 0.17), (0.07, 0.08, 0.07), (0.85, 0.68, 0.22)
write("SK_Outfit_PatrolUniform", [
    Part(torso_shell(.014, .52, 1.34), "torso", "M_Uniform"),
    Part(sleeve("l", .012, .97), "arm_l", "M_Uniform"),
    Part(sleeve("r", .012, .97), "arm_r", "M_Uniform"),
    Part(torso_shell(.038, .66, 1.27), "torso", "M_Vest"),
    Part(back_text("BORDER", 1.12, .043), "torso", "M_Letters"),
    Part(back_text("PATROL", .985, .043), "torso", "M_Letters"),
    Part(ellipsoid(surface_pt(np.pi / 2 - .55, 1.08, .05), (.035, .045, .012), 14, 6), "torso", "M_Badge"),
    Part(ellipsoid(surface_pt(np.pi / 2 + .5, .80, .055), (.06, .05, .03), 12, 6), "torso", "M_Vest"),
    Part(ellipsoid(surface_pt(np.pi / 2 - .5, .80, .055), (.06, .05, .03), 12, 6), "torso", "M_Vest"),
], [M("M_Uniform", green), M("M_Vest", dark), M("M_Letters", (0.95, 0.8, 0.2)), M("M_Badge", gold, .9, .3)])

write("SK_Outfit_PatrolPants", [
    Part(hips_shell(.016), "hips", "M_Uniform"),
    Part(pant_leg("l", .014), "leg_l", "M_Uniform"),
    Part(pant_leg("r", .014), "leg_r", "M_Uniform"),
    Part(belt(.62, .035, .028), "hips", "M_DutyBelt"),
], [M("M_Uniform", green), M("M_DutyBelt", dark)])

# Ball cap with bill + gold patch.
capv, capf = cap(lambda th, ph: np.full_like(th, HR + .018), lambda th: 1.60 + .06 * np.sin(th), S=48)
th = np.linspace(np.pi / 2 - 1.0, np.pi / 2 + 1.0, 24); rows = []
for t in np.linspace(0, 1, 6):
    reach = .15 * t * np.cos((th - np.pi / 2) * 1.2) ** .5
    rows.append(np.stack([np.cos(th) * (HR + .02) + np.cos(th) * reach * 0, 1.665 - .03 * t + 0 * th, np.sin(th) * (HR + .02) + reach], 1))
bill = sheet(np.array(rows)); billv2 = bill[0] + [0, .01, 0]
bill = (np.vstack([bill[0], billv2]), np.vstack([bill[1], bill[1][:, ::-1] + len(bill[0])]))
patch = ellipsoid(HC + [0, .17, HR - .01], (.05, .035, .015), 12, 6)
write("SK_Outfit_PatrolCap", [Part((capv, capf), "hat", "M_Cap"), Part(bill, "hat", "M_Cap"), Part(patch, "hat", "M_Patch")],
      [M("M_Cap", green), M("M_Patch", gold, .8, .35)])

# Sunglasses: two lenses + bridge + arms.
lens = merge(ellipsoid((.10, 1.585, .305), (.06, .04, .012), 16, 8), ellipsoid((-.10, 1.585, .305), (.06, .04, .012), 16, 8))
frame = merge(tube([(-.045, 1.595, .31), (0, 1.60, .315), (.045, 1.595, .31)], [.006], S=6, n=2),
              tube([(.16, 1.59, .29), (.27, 1.59, .10), (.29, 1.58, -.02)], [.006], S=6, n=2),
              tube([(-.16, 1.59, .29), (-.27, 1.59, .10), (-.29, 1.58, -.02)], [.006], S=6, n=2))
write("SK_Outfit_Sunglasses", [Part(lens, "patch", "M_Lens"), Part(frame, "hat", "M_Frame")],
      [M("M_Lens", (0.02, 0.02, 0.03), .6, .1), M("M_Frame", (0.05, 0.05, 0.05), .7, .3)])
