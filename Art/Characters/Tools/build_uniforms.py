"""Special uniforms (NOT in the normal shirt/pants pickers - only certain people wear them).

  SK_Uniform_PoliciaShirt / SK_Uniform_PoliciaPants   - Mexican police (Policia) - only the Policia wear these
  SK_Uniform_MariachiJacket / SK_Uniform_MariachiPants - charro suit: the mariachi band, and anyone holding a trumpet

Same skeleton + body shape keys as the other outfits. Run from this folder:  python build_uniforms.py   (writes into ../Outfits)
"""
import numpy as np
exec(open('build_outfits.py').read().split('# ------------------------------------------------------------------ RUNNER SET')[0])
OUT = "../Outfits"

FONT.update({
 'I': ["11111","00100","00100","00100","00100","00100","11111"],
 'C': ["01111","10000","10000","10000","10000","10000","01111"],
})

# ------------------------------------------------------------------ POLICIA
navy, black, gold, white = (0.07, 0.11, 0.30), (0.04, 0.04, 0.05), (0.85, 0.68, 0.22), (0.92, 0.92, 0.9)
pocket_l = ellipsoid(surface_pt(np.pi / 2 - .45, 1.07, .03), (.05, .055, .012), 12, 6)
pocket_r = ellipsoid(surface_pt(np.pi / 2 + .45, 1.07, .03), (.05, .055, .012), 12, 6)
shoulder_l = ellipsoid((.24, 1.205, 0), (.07, .015, .05), 12, 6)
shoulder_r = ellipsoid((-.24, 1.205, 0), (.07, .015, .05), 12, 6)
write("SK_Uniform_PoliciaShirt", [
    Part(torso_shell(.015, .52, 1.33), "torso", "M_Uniform"),
    Part(sleeve("l", .013, .97), "arm_l", "M_Uniform"),
    Part(sleeve("r", .013, .97), "arm_r", "M_Uniform"),
    Part(torso_shell(.022, 1.15, 1.31, n=8), "torso", "M_Collar"),
    Part(pocket_l, "torso", "M_Uniform"), Part(pocket_r, "torso", "M_Uniform"),
    Part(shoulder_l, "torso", "M_Collar"), Part(shoulder_r, "torso", "M_Collar"),
    Part(ellipsoid(surface_pt(np.pi / 2 - .45, 1.13, .045), (.035, .045, .012), 14, 6), "torso", "M_Badge"),
    Part(back_text("POLICIA", 1.10, .03), "torso", "M_Letters"),
], [M("M_Uniform", navy), M("M_Collar", black), M("M_Badge", gold, .9, .3), M("M_Letters", white)])

write("SK_Uniform_PoliciaPants", [
    Part(hips_shell(.016), "hips", "M_Uniform"),
    Part(pant_leg("l", .014), "leg_l", "M_Uniform"),
    Part(pant_leg("r", .014), "leg_r", "M_Uniform"),
    Part(belt(.62, .035, .03), "hips", "M_DutyBelt"),
    Part(ellipsoid((.24, .60, .02), (.04, .07, .05), 12, 6), "hips", "M_DutyBelt"),   # holster
    Part(ellipsoid((-.22, .60, .06), (.035, .04, .03), 12, 6), "hips", "M_DutyBelt"), # radio
], [M("M_Uniform", navy), M("M_DutyBelt", black)])

# ------------------------------------------------------------------ MARIACHI (charro suit)
silver, red, shirt = (0.82, 0.82, 0.85), (0.75, 0.06, 0.06), (0.95, 0.94, 0.9)
def studs_along(points, out, n, size=.016):
    P = catmull(points, 8)
    idx = np.linspace(0, len(P) - 1, n).astype(int)
    return merge(*[ellipsoid(tuple(P[i] + out), (size, size, size), 8, 5) for i in idx])
# Short black jacket (ends at the waist) over a white shirt, silver buttons down the front, red bow tie.
buttons = merge(*[ellipsoid(surface_pt(np.pi / 2 + a, y, .04), (.016, .016, .01), 8, 5) for a in (-.32, .32) for y in np.linspace(.92, 1.18, 4)])
bow = merge(ellipsoid((.045, 1.27, float(env_r(np.array([1.27]))[0]) * DEPTH + .04), (.045, .025, .015), 10, 6),
            ellipsoid((-.045, 1.27, float(env_r(np.array([1.27]))[0]) * DEPTH + .04), (.045, .025, .015), 10, 6),
            ellipsoid((0, 1.27, float(env_r(np.array([1.27]))[0]) * DEPTH + .045), (.018, .018, .015), 8, 5))
write("SK_Uniform_MariachiJacket", [
    Part(torso_shell(.012, .55, 1.30), "torso", "M_ShirtWhite"),
    Part(torso_shell(.026, .86, 1.32), "torso", "M_Charro"),
    Part(sleeve("l", .02, .97), "arm_l", "M_Charro"),
    Part(sleeve("r", .02, .97), "arm_r", "M_Charro"),
    Part(studs_along(ARM["l"], np.array([0, .045, 0]), 7), "arm_l", "M_Silver"),
    Part(studs_along(ARM["r"], np.array([0, .045, 0]), 7), "arm_r", "M_Silver"),
    Part(buttons, "torso", "M_Silver"),
    Part(bow, "torso", "M_Bow"),
], [M("M_ShirtWhite", shirt), M("M_Charro", (0.03, 0.03, 0.035)), M("M_Silver", silver, .9, .25), M("M_Bow", red)])

write("SK_Uniform_MariachiPants", [
    Part(hips_shell(.016), "hips", "M_Charro"),
    Part(pant_leg("l", .016), "leg_l", "M_Charro"),
    Part(pant_leg("r", .016), "leg_r", "M_Charro"),
    Part(studs_along(LEG["l"], np.array([.075, 0, 0]), 8), "leg_l", "M_Silver"),
    Part(studs_along(LEG["r"], np.array([-.075, 0, 0]), 8), "leg_r", "M_Silver"),
    Part(belt(.62, .034, .022), "hips", "M_Silver"),
], [M("M_Charro", (0.03, 0.03, 0.035)), M("M_Silver", silver, .9, .25)])
