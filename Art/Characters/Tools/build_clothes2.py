"""More clothes and glasses (same skeleton + body shape keys as the other outfits).

Shirts:  SK_Shirt_TShirt, SK_Shirt_TankTop, SK_Shirt_Flannel, SK_Shirt_Hoodie, SK_Shirt_Guayabera
Pants:   SK_Pants_Shorts, SK_Pants_Cargo, SK_Pants_Sweatpants
Glasses: SK_Glasses_Aviator, SK_Glasses_Round, SK_Glasses_Shades
Run from this folder:  python build_clothes2.py   (writes into ../Outfits)
"""
import numpy as np
exec(open('build_outfits.py').read().split('# ------------------------------------------------------------------ RUNNER SET')[0])
OUT = "../Outfits"

# ------------------------------------------------------------------ SHIRTS (material "M_Shirt" = the colour slider tints it)
write("SK_Shirt_TShirt", [
    Part(torso_shell(.013, .55, 1.30), "torso", "M_Shirt"),
    Part(sleeve("l", .013, .30, flare=1.12), "arm_l", "M_Shirt"),
    Part(sleeve("r", .013, .30, flare=1.12), "arm_r", "M_Shirt"),
    Part(ring_torus(1.29, float(env_r(np.array([1.29]))[0]) + .012, .008, S=48, s=6), "torso", "M_Trim"),
], [M("M_Shirt", (0.9, 0.9, 0.88)), M("M_Trim", (0.75, 0.75, 0.72))])

write("SK_Shirt_TankTop", [
    Part(torso_shell(.012, .55, 1.22), "torso", "M_Shirt"),
], [M("M_Shirt", (0.95, 0.95, 0.93))])

pocket_l = ellipsoid(surface_pt(np.pi / 2 - .45, 1.07, .03), (.05, .055, .012), 12, 6)
pocket_r = ellipsoid(surface_pt(np.pi / 2 + .45, 1.07, .03), (.05, .055, .012), 12, 6)
write("SK_Shirt_Flannel", [
    Part(torso_shell(.016, .50, 1.33), "torso", "M_Shirt"),
    Part(sleeve("l", .015, .97), "arm_l", "M_Shirt"),
    Part(sleeve("r", .015, .97), "arm_r", "M_Shirt"),
    Part(torso_shell(.024, 1.15, 1.31, n=8), "torso", "M_ShirtCollar"),
    Part(pocket_l, "torso", "M_ShirtCollar"), Part(pocket_r, "torso", "M_ShirtCollar"),
], [M("M_Shirt", (0.6, 0.12, 0.1)), M("M_ShirtCollar", (0.12, 0.08, 0.07))])

# Hoodie: thick, long sleeves, hood bunched behind the neck, front pocket.
hood = ellipsoid((0, 1.33, -.13), (.2, .13, .12), 20, 10)
kanga = ellipsoid(surface_pt(np.pi / 2, .78, .04), (.16, .07, .02), 16, 6)
write("SK_Shirt_Hoodie", [
    Part(torso_shell(.03, .48, 1.32), "torso", "M_Shirt"),
    Part(sleeve("l", .025, .97, flare=.9), "arm_l", "M_Shirt"),
    Part(sleeve("r", .025, .97, flare=.9), "arm_r", "M_Shirt"),
    Part(hood, "torso", "M_Shirt"),
    Part(kanga, "torso", "M_ShirtDark"),
    Part(tube([(.05, 1.25, .2), (.06, 1.10, .26)], [.006], S=6, n=2), "torso", "M_ShirtDark"),
    Part(tube([(-.05, 1.25, .2), (-.06, 1.10, .26)], [.006], S=6, n=2), "torso", "M_ShirtDark"),
], [M("M_Shirt", (0.35, 0.36, 0.4)), M("M_ShirtDark", (0.2, 0.2, 0.22))])

# Guayabera: light short-sleeve shirt with two rows of pleats + four pockets.
pleats = merge(*[tube([surface_pt(np.pi / 2 + a, 1.24, .02), surface_pt(np.pi / 2 + a, .60, .02)], [.005], S=5, n=3)
                 for a in (-.6, -.5, .5, .6)])
pockets = merge(*[ellipsoid(surface_pt(np.pi / 2 + a, y, .028), (.045, .045, .01), 10, 5) for a in (-.45, .45) for y in (1.06, .74)])
write("SK_Shirt_Guayabera", [
    Part(torso_shell(.016, .50, 1.31), "torso", "M_Shirt"),
    Part(sleeve("l", .015, .35, flare=1.1), "arm_l", "M_Shirt"),
    Part(sleeve("r", .015, .35, flare=1.1), "arm_r", "M_Shirt"),
    Part(pleats, "torso", "M_ShirtTrim"), Part(pockets, "torso", "M_ShirtTrim"),
], [M("M_Shirt", (0.93, 0.9, 0.8)), M("M_ShirtTrim", (0.85, 0.82, 0.72))])

# ------------------------------------------------------------------ PANTS (material "M_Pants")
write("SK_Pants_Shorts", [
    Part(hips_shell(.016), "hips", "M_Pants"),
    Part(pant_leg("l", .022, .42), "leg_l", "M_Pants"),
    Part(pant_leg("r", .022, .42), "leg_r", "M_Pants"),
    Part(belt(.62, .033, .02), "hips", "M_Belt"),
], [M("M_Pants", (0.65, 0.55, 0.38)), M("M_Belt", (0.2, 0.12, 0.06))])

cargo_l = ellipsoid((.18, .30, .03), (.03, .07, .055), 12, 6)
cargo_r = ellipsoid((-.18, .30, .03), (.03, .07, .055), 12, 6)
write("SK_Pants_Cargo", [
    Part(hips_shell(.02), "hips", "M_Pants"),
    Part(pant_leg("l", .03), "leg_l", "M_Pants"),
    Part(pant_leg("r", .03), "leg_r", "M_Pants"),
    Part(cargo_l, "leg_l", "M_Pants"), Part(cargo_r, "leg_r", "M_Pants"),
    Part(belt(.62, .036, .022), "hips", "M_Belt"),
], [M("M_Pants", (0.3, 0.33, 0.22)), M("M_Belt", (0.1, 0.1, 0.1))])

def cuff(s):
    P = catmull(LEG[s], 8)
    return ring_torus(float(P[-1][1]) + .03, .065, .02, S=24, s=6, depth=1.0)
cl, cr = cuff("l"), cuff("r")
cl = (cl[0] + [LEG["l"][-1][0], 0, 0], cl[1]); cr = (cr[0] + [LEG["r"][-1][0], 0, 0], cr[1])
write("SK_Pants_Sweatpants", [
    Part(hips_shell(.024), "hips", "M_Pants"),
    Part(pant_leg("l", .028), "leg_l", "M_Pants"),
    Part(pant_leg("r", .028), "leg_r", "M_Pants"),
    Part(cl, "leg_l", "M_PantsCuff"), Part(cr, "leg_r", "M_PantsCuff"),
    Part(tube([(0, .62, .2), (.015, .55, .22)], [.005], S=6, n=2), "hips", "M_PantsCuff"),
], [M("M_Pants", (0.25, 0.27, 0.33)), M("M_PantsCuff", (0.18, 0.19, 0.23))])

# ------------------------------------------------------------------ GLASSES (worn on the head; follow the face shapes)
def arms(y, z, col):
    return merge(tube([(.185, y, z - .02), (.27, y, .10), (.29, y - .01, -.02)], [.006], S=6, n=2),
                 tube([(-.185, y, z - .02), (-.27, y, .10), (-.29, y - .01, -.02)], [.006], S=6, n=2))

# Aviators: teardrop gold-rimmed lenses.
def teardrop(x):
    return ellipsoid((x, 1.599, .287), (.062, .05, .01), 18, 8)
av_lens = merge(teardrop(.125), teardrop(-.125))
def tear_rim(x):
    pts = []
    for a in np.linspace(0, 2 * np.pi, 21):
        r = .06 + .012 * max(0., -np.sin(a))          # wider at the bottom (teardrop)
        pts.append((x + r * np.cos(a) * 1.05, 1.599 + r * np.sin(a) * .8, .292))
    return tube(pts, [.0045], S=5, n=1, caps=False)
av_frame = merge(tear_rim(.125), tear_rim(-.125), tube([(-.065, 1.621, .292), (0, 1.626, .297), (.065, 1.621, .292)], [.004], S=6, n=2), arms(1.616, .287, None))
write("SK_Glasses_Aviator", [Part(av_lens, "patch", "M_Lens"), Part(av_frame, "hat", "M_Frame")],
      [M("M_Lens", (0.25, 0.18, 0.05), .7, .08), M("M_Frame", (0.85, 0.68, 0.25), .9, .25)])

# Round reading glasses: thin dark frames, clear-ish lenses.
rnd_lens = merge(ellipsoid((.125, 1.606, .287), (.042, .042, .006), 16, 6), ellipsoid((-.125, 1.606, .287), (.042, .042, .006), 16, 6))
rim = lambda x: tube([(x + .045 * np.cos(a), 1.606 + .045 * np.sin(a), .292) for a in np.linspace(0, 2 * np.pi, 17)], [.005], S=5, n=1, caps=False)
write("SK_Glasses_Round", [Part(rnd_lens, "patch", "M_Lens"), Part(merge(rim(.125), rim(-.125),
      tube([(-.08, 1.616, .292), (0, 1.621, .297), (.08, 1.616, .292)], [.004], S=6, n=2), arms(1.616, .287, None)), "hat", "M_Frame")],
      [M("M_Lens", (0.75, 0.85, 0.9), .2, .05), M("M_Frame", (0.08, 0.06, 0.05), .3, .4)])

# Classic sunglasses (wayfarer style): two dark rounded-square lenses, thick black frame, bridge over the nose.
def _shade_pt(cx, t, a=.068, b=.048, n=4.):
    c, sn = np.cos(t), np.sin(t)
    y = b * np.sign(sn) * np.abs(sn) ** (2 / n)
    x = a * np.sign(c) * np.abs(c) ** (2 / n) * (1 + .1 * y / b)       # a little wider at the top
    X = cx + x
    z = .296 - .45 * max(0., abs(X) - .1) ** 1.3                        # wraps back around the face a bit
    return np.array([X, 1.603 + y, z])


def shade_lens(cx):
    ts = np.linspace(0, 2 * np.pi, 33)[:-1]
    rim = np.array([_shade_pt(cx, t, .064, .044) for t in ts])
    c = rim.mean(0) + [0, 0, .004]
    V = np.vstack([[c], rim]); F = np.array([(0, 1 + i, 1 + (i + 1) % len(rim)) for i in range(len(rim))])
    V2 = V - [0, 0, .004]                                               # back face too
    return np.vstack([V, V2]), np.vstack([F, F[:, ::-1] + len(V)])


def shade_frame(cx):
    ts = np.linspace(0, 2 * np.pi, 41)
    rim = tube([_shade_pt(cx, t) for t in ts], [.0075], S=6, n=1, caps=False)
    brow = tube([_shade_pt(cx, t) + [0, .006, .002] for t in np.linspace(.15, np.pi - .15, 12)], [.0095], S=6, n=2)  # thick top bar
    return merge(rim, brow)


bridge = tube([(-.058, 1.622, .294), (0, 1.627, .302), (.058, 1.622, .294)], [.0065], S=6, n=3)
write("SK_Glasses_Shades", [Part(merge(shade_lens(.125), shade_lens(-.125)), "patch", "M_Lens"),
                            Part(merge(shade_frame(.125), shade_frame(-.125), bridge, arms(1.62, .276, None)), "hat", "M_Frame")],
      [M("M_Lens", (0.03, 0.03, 0.035), .6, .08), M("M_Frame", (0.02, 0.02, 0.02), .2, .35)])
