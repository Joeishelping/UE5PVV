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


def strands(th, ph, t):
    """Locks of hair: grooves running from the crown down to the hairline (with a little swirl),
    deeper where the hair is thicker. Short buzz cuts get only a faint texture."""
    a = th + .18 * np.sin(ph * 4 + th * 2)
    lock = .5 + .5 * np.sin(a * 34 + 2.1 * np.sin(a * 7))
    fine = .5 + .5 * np.sin(a * 110 + 1.3 * np.sin(ph * 9))
    depth = np.clip(t * .4, .0008, .013)
    return depth * (.75 * lock ** 2 + .25 * fine)  # only ever adds (never digs below the scalp)


def ragged(hairline):
    """Uneven hairline: little tufts instead of a perfectly clean edge (so it doesn't read as a cap)."""
    return lambda th: hairline(th) + .004 * np.sin(th * 37) + .003 * np.sin(th * 61 + 1.3)


def shell(thick, hairline, S=200, rings=28, detail=1., minimum=.006):
    return cap(lambda th, ph: HR + np.maximum(thick(th, ph), minimum) + detail * strands(th, ph, thick(th, ph)), ragged(hairline), S=S, rings=rings)


TEX_STRAIGHT = None
TEX_CURLY = None
CURLY_STYLES = {"SK_Hair_Afro", "SK_Hair_CurlyTop", "SK_Hair_Twists", "SK_Hair_Dreads"}


def hair_textures(name):
    global TEX_STRAIGHT, TEX_CURLY
    import hair_tex
    if name in CURLY_STYLES:
        TEX_CURLY = TEX_CURLY or hair_tex.curly()
        return {"M_Hair": TEX_CURLY}
    TEX_STRAIGHT = TEX_STRAIGHT or hair_tex.straight()
    return {"M_Hair": TEX_STRAIGHT}


def write_hair(name, parts):
    path = os.path.join(OUT, name + ".glb")
    write_glb(path, [Part(vf, "hair", "M_Hair") for vf in parts], HAIR_MAT, textures=hair_textures(name), uv_fn=hair_uv)
    print("wrote", path)


def human_line(front=1.735, back=1.43, temples=.03):
    """A real hairline: up on the forehead, a little higher still at the temples (the corners), down at the nape."""
    return lambda th: hairline_std(th, front, back) + temples * np.exp(-((np.sin(th) - .62) / .17) ** 2)


std = human_line()
# Low sides so short cuts cover the temples (ears still show).
low = human_line(1.73, 1.40, .025)

# ------------------------------------------------------------------ shared
# ------------------------------------------------------------------ fades & modern cuts
def fade_band(top, bottom, density=1., S=220, rows=18, seed=3):
    """A skin fade: a thin layer of hair between the bottom line (skin) and the top line (full hair), with little
    patches dropped at random - sparse at the bottom, solid at the top - so it fades into the skin."""
    rng = np.random.default_rng(seed)
    th = TH(S)
    y0, y1 = bottom(th), top(th)
    ph0 = np.arcsin(np.clip((y0 - HC[1]) / HR, -.99, .99))
    ph1 = np.arcsin(np.clip((y1 - HC[1]) / HR, -.99, .99))
    rings = []
    for k in np.linspace(0, 1, rows):
        ph = ph0 + (ph1 - ph0) * k + .015
        rings.append(sph(th, ph, np.full(S, HR + .0035)))
    V, F = grid(rings, None, None)
    row = (F[:, 0] // S) / (rows - 1)                                 # 0 = bottom (skin) .. 1 = top (hair)
    tall = np.abs(y1 - y0)[F[:, 0] % S] > .015                       # no band where the lines meet (forehead)
    keep = (rng.random(len(F)) < np.clip(row ** .8 * density, 0, 1)) & tall
    return V, F[keep]


buzz_top = human_line(1.73, 1.52, .025)
write_hair("SK_Hair_Buzz", [shell(lambda th, ph: np.full_like(th, .0035), buzz_top, detail=.4, minimum=.003),
                            fade_band(buzz_top, low, 1.3, seed=5)])       # a soft, short buzz that blends at the edges

# ------------------------------------------------------------------ more cuts


write_hair("SK_Hair_SlickBack", [shell(
    lambda th, ph: .022 + .022 * top(ph, .6, .5) + .015 * (1 - front(th)) * ss((1.1 - ph) / .6)
    + .003 * np.sin(th * 26) * top(ph, .5, .4), std)])




def mullet_line(th):
    # Short at the front, long party in the back down to the neck.
    return 1.27 + (1.66 - 1.27) * front(th) ** .7


write_hair("SK_Hair_Mullet", [shell(
    lambda th, ph: .02 + .015 * top(ph) + .05 * (1 - front(th)) ** 2 * ss((.2 - ph) / .6), mullet_line, rings=22)])


def long_line(th):
    return 1.69 * front(th) ** 3 + 1.24 * (1 - front(th) ** 3)


write_hair("SK_Hair_LongWavy", [shell(
    lambda th, ph: .025 + .07 * ss((.4 - ph) / .9) * (1 - .6 * front(th)) + .008 * np.sin(th * 14 + ph * 6),
    long_line)])

# ------------------------------------------------------------------ regulation cuts







def side_part(th, ph):
    x = np.cos(ph) * np.cos(th) * HR
    groove = .014 * np.exp(-((x - .09) / .012) ** 2) * top(ph, .5, .3)
    sweep = .012 * ss(-x / .2) * top(ph, .5, .5)  # combed over to one side
    return .02 + .012 * top(ph, .6, .5) + sweep - groove


write_hair("SK_Hair_SidePart", [shell(side_part, std)])


# ------------------------------------------------------------------ volume styles
afro_bump = lambda th, ph: .01 * np.sin(th * 13) * np.sin(ph * 11)
write_hair("SK_Hair_Afro", [shell(lambda th, ph: .06 + .07 * ss((ph + .2) / 1.2) + afro_bump(th, ph), std, detail=.25)])

waves = lambda th, ph: .0025 * np.sin(np.arccos(np.clip(np.cos(ph) * np.sin(th), -1, 1)) * 40)  # rings around the crown
write_hair("SK_Hair_Waves", [shell(lambda th, ph: .008 + waves(th, ph), low)])

write_hair("SK_Hair_Pompadour", [shell(
    lambda th, ph: .006 + .03 * top(ph, .7, .4) + .06 * top(ph, .8, .4) * ss((np.sin(th) - .2) / .6), low)])


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

fade_low = lambda th: hairline_std(th, 1.71, 1.40)                    # where the hair ends (skin below)
fade_mid = lambda th: hairline_std(th, 1.715, 1.58)                    # mid fade: full hair from here up
fade_high = lambda th: hairline_std(th, 1.725, 1.66)                   # high & tight


def curls(n, seed, ph_min=.85, r=(.018, .03), lift=.018):
    rng = np.random.default_rng(seed)
    parts = []
    for _ in range(n):
        ph = np.arcsin(rng.uniform(np.sin(ph_min), 1.))
        t = rng.uniform(0, 2 * np.pi)
        c = sph(np.array([t]), np.array([ph]), np.array([HR + lift + rng.uniform(0, .015)]))[0]
        rr = rng.uniform(*r)
        parts.append(ellipsoid(c, (rr, rr * .9, rr), 8, 5))
    return merge(*parts)


def spikes(n, seed, ph_min=.9, length=(.05, .085)):
    rng = np.random.default_rng(seed)
    parts = []
    for _ in range(n):
        ph = np.arcsin(rng.uniform(np.sin(ph_min), 1.))
        t = rng.uniform(0, 2 * np.pi)
        base = sph(np.array([t]), np.array([ph]), np.array([HR + .008]))[0]
        up = (base - HC) / np.linalg.norm(base - HC) + [0, .3, .15]
        up /= np.linalg.norm(up)
        L = rng.uniform(*length)
        parts.append(tube([base - up * .01, base + up * L * .5, base + up * L], [.018, .011, .002], S=7, n=2))
    return merge(*parts)


# Fade: short textured top, skin fade on the sides and back.
write_hair("SK_Hair_Fade", [shell(lambda th, ph: .018 + .02 * top(ph, .85, .3) * (.6 + .4 * front(th)), fade_mid), fade_band(fade_mid, fade_low)])
# High and tight: very short top, sides faded almost all the way up.
write_hair("SK_Hair_HighAndTight", [shell(lambda th, ph: .006 + .012 * top(ph, 1.05, .25), fade_high), fade_band(fade_high, fade_low, .8)])
# Crew cut: short all over, a bit longer at the front, low taper.
crew_line = lambda th: hairline_std(th, 1.705, 1.50)
write_hair("SK_Hair_CrewCut", [shell(lambda th, ph: .01 + .016 * top(ph, .7, .4) * (.6 + .4 * front(th)), crew_line), fade_band(crew_line, fade_low)])
# Flat top: a flat-topped block of hair, sides faded.
flat_v, flat_f = shell(lambda th, ph: .012 + .09 * top(ph, .95, .3), fade_mid)
roof = HC[1] + HR + .045
flat_v[:, 1] = np.minimum(flat_v[:, 1], roof)                         # sanded flat
write_hair("SK_Hair_FlatTop", [(flat_v, flat_f), fade_band(fade_mid, fade_low)])
# Curly top: a mop of curls on top, faded sides.
write_hair("SK_Hair_CurlyTop", [shell(lambda th, ph: np.full_like(th, .01), fade_mid), curls(150, 11), fade_band(fade_mid, fade_low)])
# Undercut: long top slicked back, sides shaved clean.
under_line = lambda th: hairline_std(th, 1.70, 1.63)
write_hair("SK_Hair_Undercut", [shell(lambda th, ph: .03 + .035 * top(ph, .9, .4) * (.5 + .5 * front(th)), under_line), fade_band(under_line, fade_low, .35)])
# Soccer mohawk ("fohawk"): sides faded short, a soft ridge of longer hair down the middle that peaks toward the front.
def fohawk(th, ph):
    x = np.cos(ph) * np.cos(th) * HR                                  # left/right across the head
    z = np.cos(ph) * np.sin(th) * HR                                  # front (+) / back (-)
    ridge = np.exp(-(x / .055) ** 2) * (.6 + .4 * np.clip((z + .1) / .3, 0, 1))
    return .008 + .06 * ridge * top(ph, .55, .45)
fohawk_line = human_line(1.725, 1.56, .03)
write_hair("SK_Hair_SoccerMohawk", [shell(fohawk, fohawk_line), fade_band(fohawk_line, fade_low, 1.1, seed=12)])
# Balding: the classic horseshoe - hair round the sides and back, bare and shiny on top.
def horseshoe(th, ph):
    return .012 + .006 * (1 - front(th))
bald_top = lambda th: 1.62 + .03 * np.clip(np.sin(th), 0, 1)        # where the bald patch starts (a bit higher in front)
def bald_ring():
    V, F = shell(horseshoe, low)
    cut = V[:, 1] < bald_top(np.arctan2(V[:, 2] - HC[2], V[:, 0] - HC[0]))
    keep = cut[F].all(1)                                                # only the ring below the bald line
    F = F[keep]
    used = np.unique(F); remap = -np.ones(len(V), int); remap[used] = np.arange(len(used))
    return V[used], remap[F]
write_hair("SK_Hair_Balding", [bald_ring(), fade_band(bald_top, lambda th: bald_top(th) + .045 * (low(th) < bald_top(th) - .01), .6, seed=17)])  # thinning just above the ring (sides/back only)
# Comb-over: the same horseshoe, plus a few long strands combed across the bald top (it fools nobody).
combs = [bald_ring()]
for k, z in enumerate(np.linspace(-.12, .14, 6)):
    pts = []
    for x in np.linspace(.2, -.17, 9):
        y2 = (HR + .012) ** 2 - x ** 2 - z ** 2
        pts.append((x, HC[1] + np.sqrt(max(y2, 1e-6)), z))
    combs.append(tube(pts, [.012, .01, .006], S=6, n=3))
write_hair("SK_Hair_CombOver", combs)

# Man bun: slicked back with a bun at the back of the head.
slick = lambda th, ph: .016 + .012 * top(ph, .6, .5)
write_hair("SK_Hair_ManBun", [shell(slick, std), ellipsoid((0., 1.71, -.30), (.085, .08, .08), 16, 10)])
# Ponytail: slicked back, tail hanging down the back.
write_hair("SK_Hair_Ponytail", [shell(slick, std), ellipsoid((0., 1.63, -.28), (.03, .03, .03), 10, 6),
                                tube([(0., 1.64, -.30), (0., 1.56, -.36), (0., 1.44, -.37), (0., 1.33, -.35)], [.045, .04, .03, .012], S=12, n=4)])
# Spiky: short base with gelled spikes all over the top.
write_hair("SK_Hair_Spiky", [shell(lambda th, ph: np.full_like(th, .012), crew_line), spikes(70, 21), fade_band(crew_line, fade_low)])
# Curtains: 90s middle part, hanging to the ears.
def curtains(th, ph):
    x = np.cos(ph) * np.cos(th) * HR
    part = .012 * np.exp(-(x / .012) ** 2) * top(ph, .8, .3)
    return .03 + .02 * (1 - top(ph, .5, .6)) + .015 * top(ph, .9, .3) - part
curtain_line = lambda th: 1.45 + .27 * np.clip(np.sin(th), 0, 1) ** 3                 # parted high in the middle, down to the ears at the sides
write_hair("SK_Hair_Curtains", [shell(curtains, curtain_line)])
# Caesar: short all over with a straight fringe across the forehead.
caesar_line = lambda th: np.minimum(human_line(1.7, 1.45, 0.)(th), 1.7)  # straight short fringe
write_hair("SK_Hair_Caesar", [shell(lambda th, ph: .014 + .006 * front(th), caesar_line)])
# Dreads: a thin cap with locks hanging all round to the shoulders.
dreads = [shell(lambda th, ph: np.full_like(th, .012), std)]
rng = np.random.default_rng(31)
for t in np.linspace(0, 2 * np.pi, 34, endpoint=False):
    if np.sin(t) > .55:
        continue                                                      # keep the face clear
    ph = rng.uniform(.25, .7)
    base = sph(np.array([t]), np.array([ph]), np.array([HR + .015]))[0]
    out = np.array([np.cos(t), 0, np.sin(t)])
    L = rng.uniform(.18, .3)
    dreads.append(tube([base, base + out * .05 - [0, .06, 0], base + out * .07 - [0, L * .6, 0], base + out * .07 - [0, L, 0]],
                       [.016, .015, .014, .011], S=8, n=3))
write_hair("SK_Hair_Dreads", dreads)

# ------------------------------------------------------------------ facial hair
# Every beard is a sheet hugging the lower face, built on a grid of (a = angle round the face, 0 = front; y = height),
# kept only where the style's MASK says, and thick where THICK says (edges thin out so they blend into the skin).
LIP_TOP, LIP_BOT, LIP_A = 1.441, 1.372, .33      # the lips (the new, smaller ones)
NOSE_Y = 1.474                                      # bottom of the nose

def _sm(x):
    x = np.clip(x, 0, 1)
    return x * x * (3 - 2 * x)


def face_sheet(mask, thick, a_max=1.45, y_lo=1.0, y_hi=1.62, cols=150, rows=90, drop=None, speckle=0.):
    """mask(a, y) -> 0..1 coverage, thick(a, y) -> metres off the skin, drop(a, y) -> metres the hair hangs down (long beards)."""
    A = np.linspace(-a_max, a_max, cols)
    Y = np.linspace(y_lo, y_hi, rows)
    AA, YY = np.meshgrid(A, Y)
    M = mask(AA, YY)
    T = thick(AA, YY) * _sm(M * 2.2)                 # thin out toward the edge of the mask
    D = drop(AA, YY) * _sm(M * 2.2) if drop else np.zeros_like(AA)
    r = HR + .002 + T
    yy = YY - D
    rr = np.sqrt(np.maximum(r ** 2 - (YY - HC[1]) ** 2, (r * .3) ** 2))
    th = np.pi / 2 + AA
    P = np.stack([rr * np.cos(th), yy, rr * np.sin(th) + D * .35], -1)
    V, F = sheet(P)
    keep = M.reshape(-1)[F].min(1) > .02
    if speckle > 0:
        rng = np.random.default_rng(7)
        keep &= rng.random(len(F)) > speckle          # stubble: only some of the little patches
    F = F[keep]
    used = np.unique(F)
    remap = -np.ones(len(V), int)
    remap[used] = np.arange(len(used))
    return V[used], remap[F]


def lips_hole(a, y, pad=.012):
    """1 = fine, 0 = on the lips (beards go round the mouth)."""
    da = np.abs(a) / (LIP_A + .04)
    dy = np.where(y > (LIP_TOP + LIP_BOT) / 2, (y - (LIP_TOP + LIP_BOT) / 2) / ((LIP_TOP - LIP_BOT) / 2 + pad),
                  ((LIP_TOP + LIP_BOT) / 2 - y) / ((LIP_TOP - LIP_BOT) / 2 + pad))
    return _sm((np.sqrt(da ** 2 + dy ** 2) - 1.) * 8)


def jaw_bottom(a):
    """Lowest line of the beard: under the chin in front, up toward the ears at the sides."""
    return 1.24 + .09 * _sm((np.abs(a) - .2) / 1.1)


def cheek_line(a, high=1.52):
    """Top of the beard on the cheeks: low by the mouth, up to the sideburns by the ears."""
    return 1.43 + (high - 1.43) * _sm((np.abs(a) - .35) / .7)


def between(v, lo, hi, soft=.012):
    return _sm((v - lo) / soft) * _sm((hi - v) / soft)


def stache_mask(width=LIP_A + .06, lo=LIP_TOP - .004, hi=NOSE_Y - .002, droop=0.):
    def m(a, y):
        k = np.clip(np.abs(a) / width, 0, 1)
        top = hi - .012 * k ** 2                     # follows under the nose, a bit lower at the ends
        bot = lo - droop * k ** 2
        return between(np.abs(a), -1, width, .03) * between(y, bot, top, .006)
    return m


def full_mask(high=1.52, mustache=True, bottom=jaw_bottom):
    def m(a, y):
        beard = between(y, bottom(a), cheek_line(a, high), .015) * between(np.abs(a), -1, 1.32, .05) * lips_hole(a, y)
        if mustache:
            beard = np.maximum(beard, stache_mask()(a, y))
        return beard
    return m


def write_face(name, parts):
    path = os.path.join(OUT, name + ".glb")
    write_glb(path, [Part(vf, "facehair", "M_Hair") for vf in parts], HAIR_MAT, textures=hair_textures(name), uv_fn=facehair_uv)
    print("wrote", path)


def curl_tip(sx, size=.011):
    """Handlebar tip: continues out from the end of the mustache, then curls UP (3/4 of a turn)."""
    a0 = LIP_A + .04
    pts = []
    for t in np.linspace(0, 1, 10):
        ang = t * 4.4
        a = a0 + .05 * t + (size / HR) * np.sin(ang)
        y = 1.452 + size * (1 - np.cos(ang)) - .004
        x = sx * np.sin(a) * (HR + .012)
        z2 = (HR + .012) ** 2 - x ** 2 - (y - HC[1]) ** 2
        pts.append((x, y, np.sqrt(max(z2, 1e-6))))
    return tube(pts, [.0075, .006, .0035], S=8, n=3)


def hang_tube(sx, y0=1.43, y1=1.27, a=LIP_A + .02):
    """A strand hanging down by the corner of the mouth (horseshoe / fu manchu)."""
    pts = []
    for y in np.linspace(y0, y1, 7):
        x = sx * np.sin(a) * HR
        z2 = (HR + .013) ** 2 - x ** 2 - (y - HC[1]) ** 2
        pts.append((x, y, np.sqrt(max(z2, 1e-6))))
    return pts


thick_full = lambda a, y: .022 + .012 * _sm((1.33 - y) / .1)
write_face("SK_Face_FullBeard", [face_sheet(full_mask(), thick_full)])
write_face("SK_Face_BoxedBeard", [face_sheet(full_mask(high=1.5), lambda a, y: np.full_like(a, .011))])
write_face("SK_Face_Stubble", [face_sheet(full_mask(high=1.5), lambda a, y: np.full_like(a, .0028), cols=220, rows=130, speckle=.45)])
write_face("SK_Face_LongBeard", [face_sheet(full_mask(), thick_full,
                                            drop=lambda a, y: .16 * _sm((1.33 - y) / .1) * np.exp(-(a / .55) ** 2))])
write_face("SK_Face_Mustache", [face_sheet(stache_mask(), lambda a, y: np.full_like(a, .012), y_lo=1.40, y_hi=1.5)])
write_face("SK_Face_Pencil", [face_sheet(stache_mask(width=LIP_A, lo=1.452, hi=1.462), lambda a, y: np.full_like(a, .004), y_lo=1.43, y_hi=1.48)])
write_face("SK_Face_Walrus", [face_sheet(stache_mask(width=LIP_A + .12, lo=LIP_TOP - .03, droop=.02),
                                         lambda a, y: .022 + .01 * _sm((y - 1.42) / .04), y_lo=1.38, y_hi=1.5)])
write_face("SK_Face_Handlebar", [face_sheet(stache_mask(width=LIP_A + .02), lambda a, y: np.full_like(a, .011), y_lo=1.40, y_hi=1.5),
                                 curl_tip(1), curl_tip(-1)])
write_face("SK_Face_Horseshoe", [face_sheet(stache_mask(), lambda a, y: np.full_like(a, .012), y_lo=1.40, y_hi=1.5),
                                 tube(hang_tube(1), [.011], S=8, n=3), tube(hang_tube(-1), [.011], S=8, n=3)])
write_face("SK_Face_FuManchu", [face_sheet(stache_mask(width=LIP_A + .02, lo=1.448), lambda a, y: np.full_like(a, .006), y_lo=1.42, y_hi=1.5),
                                tube(hang_tube(1, 1.445, 1.27), [.007, .006, .003], S=6, n=3),
                                tube(hang_tube(-1, 1.445, 1.27), [.007, .006, .003], S=6, n=3)])
# Goatee = mustache + chin, joined round the mouth (a "circle beard").
goatee = lambda a, y: np.maximum(stache_mask()(a, y),
                                 between(np.abs(a), -1, LIP_A + .07, .04) * between(y, 1.235, LIP_TOP - .01, .012) * lips_hole(a, y))
write_face("SK_Face_Goatee", [face_sheet(goatee, lambda a, y: .014 + .01 * _sm((1.32 - y) / .06))])
# Van Dyke: pointy chin beard + separate mustache.
vandyke = lambda a, y: np.maximum(stache_mask()(a, y), between(np.abs(a), -1, .2 - .55 * np.maximum(0, 1.33 - y), .03) * between(y, 1.2, LIP_BOT - .012, .01))
write_face("SK_Face_VanDyke", [face_sheet(vandyke, lambda a, y: .013 + .012 * _sm((1.3 - y) / .07))])
write_face("SK_Face_SoulPatch", [face_sheet(lambda a, y: between(np.abs(a), -1, .07, .02) * between(y, 1.335, LIP_BOT - .01, .008),
                                            lambda a, y: np.full_like(a, .008), y_lo=1.3, y_hi=1.4)])
# Chin strap: a band along the jaw line from ear to ear (no mustache).
strap = lambda a, y: between(y, jaw_bottom(a), jaw_bottom(a) + .045 + .05 * _sm((np.abs(a) - .9) / .4), .01) * between(np.abs(a), -1, 1.32, .05)
write_face("SK_Face_ChinStrap", [face_sheet(strap, lambda a, y: np.full_like(a, .01))])
# Mutton chops: big sideburns over the cheeks to the jaw, bare chin.
chops = lambda a, y: between(np.abs(a), 1.02 - 2.2 * np.maximum(0, 1.5 - y), 1.3, .05) * between(y, jaw_bottom(a) + .01, 1.54, .015)  # narrow sideburns widening down the cheeks
write_face("SK_Face_MuttonChops", [face_sheet(chops, lambda a, y: np.full_like(a, .016))])

# ---------------- connected beards & goatees (mustache joined down to the chin)
def corners(width=.07, lo=1.27):
    """The two strips down the corners of the mouth (just outside the lips) joining the mustache to the chin."""
    return lambda a, y: between(np.abs(a), LIP_A + .03, LIP_A + .03 + width, .015) * between(y, lo, 1.458, .008)
chin_piece = lambda w, lo, hi: (lambda a, y: between(np.abs(a), -1, LIP_A + .03 + w, .02) * between(y, lo, hi, .008) * lips_hole(a, y))
circle = lambda a, y: np.maximum.reduce([stache_mask()(a, y), corners(.08)(a, y), chin_piece(.08, 1.25, LIP_BOT - .006)(a, y)])
write_face("SK_Face_CircleBeard", [face_sheet(circle, lambda a, y: .016 + .006 * _sm((1.33 - y) / .06))])
ext = lambda a, y: np.maximum(circle(a, y), between(y, jaw_bottom(a), jaw_bottom(a) + .07, .012) * between(np.abs(a), -1, .8, .06))
write_face("SK_Face_ExtendedGoatee", [face_sheet(ext, lambda a, y: .015 + .006 * _sm((1.33 - y) / .06))])
strapc = lambda a, y: np.maximum.reduce([strap(a, y), stache_mask()(a, y), corners(.045, jaw_bottom(LIP_A))(a, y)])
write_face("SK_Face_ChinStrapConnected", [face_sheet(strapc, lambda a, y: np.full_like(a, .011))])
thin = lambda a, y: np.maximum.reduce([stache_mask(width=LIP_A + .07, lo=1.446)(a, y), corners(.035, 1.29)(a, y),
                                       between(np.abs(a), -1, LIP_A + .065, .015) * between(y, 1.29, 1.318, .006)])
write_face("SK_Face_ThinGoatee", [face_sheet(thin, lambda a, y: np.full_like(a, .007), y_lo=1.22, y_hi=1.5, cols=360, rows=170)])
