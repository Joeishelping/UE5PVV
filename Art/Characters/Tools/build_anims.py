"""Animation pack for the PVV body skeleton (UE mannequin bone names).

Writes ../SK_PVV_Animations.glb: the body + one glTF animation per move (Idle, Walk, Run, CrouchIdle, ...).
Import it into Unreal with Skeleton = your body's skeleton; each animation becomes an Animation Sequence.

glTF space: Y up, character faces +Z, character's left = +X. Rest pose has no rotations, so a bone's
animation rotation is simply its local rotation.
  rx(+a) on spine   = lean forward      rx(-a) on thigh/upperarm = swing forward
  rx(+a) on calf    = bend knee         rx(-a) on lowerarm       = bend elbow (forearm forward/up)
  rz(+a) on left arm = raise sideways   (right arm: use -a)
Run:  python build_anims.py
"""
import json, struct, math, sys
import numpy as np
sys.path.insert(0, ".")
from add_face_morphs import load, save

FPS = 30
BODY = "../SK_PVV_Body_Custom.glb"
OUT = "../SK_PVV_Animations.glb"


def quat_axis(axis, deg):
    a = math.radians(deg) / 2
    s = math.sin(a)
    return np.array([axis[0] * s, axis[1] * s, axis[2] * s, math.cos(a)])


def qmul(a, b):  # a*b (apply b first), xyzw
    ax, ay, az, aw = a
    bx, by, bz, bw = b
    return np.array([aw * bx + ax * bw + ay * bz - az * by,
                     aw * by - ax * bz + ay * bw + az * bx,
                     aw * bz + ax * by - ay * bx + az * bw,
                     aw * bw - ax * bx - ay * by - az * bz])


def rot(x=0., y=0., z=0.):
    """Apply Z (roll/sideways) first, then X (pitch), then Y (yaw)."""
    return qmul(quat_axis((0, 1, 0), y), qmul(quat_axis((1, 0, 0), x), quat_axis((0, 0, 1), z)))


S = math.sin
C = math.cos
TAU = 2 * math.pi
ARM_DOWN = 31.0  # rest pose arms are spread ~31 deg; this brings them to the sides


def arm(side, x=0., y=0., z=0.):
    """Upper arm: z=0 hangs at the side; +z raises outward for either side; +y swings the arm inward (across the body)."""
    k = 1 if side == "l" else -1
    return rot(x, -y * k, (z - ARM_DOWN) * k)


def legs_ik(drop, swing=0.):
    """Thigh/calf pitch so the foot stays under the hip when the pelvis drops by `drop` metres."""
    L1, L2 = .19, .15
    D = max(.12, min(L1 + L2 - .001, (L1 + L2) - drop))
    knee = math.degrees(math.acos((L1 * L1 + L2 * L2 - D * D) / (2 * L1 * L2)))
    hip = math.degrees(math.acos((L1 * L1 + D * D - L2 * L2) / (2 * L1 * D)))
    return -hip + swing, 180 - knee


# --------------------------------------------------------------------------------------- poses
def pose_stand(t, p):
    pass


def base(p):
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, 0, 0, 8)
        p[f"lowerarm_{s}"] = rot(-12)


def walk(t, p, speed=1.0, crouch=0.):
    ph = t * TAU
    amp = 26 * speed
    base(p)
    drop = crouch + .015 * (1 - C(2 * ph))
    p["pelvis_T"] = (0, -drop, 0)
    p["pelvis"] = rot(0, 6 * S(ph) * speed, 0)
    p["spine_01"] = rot(4 * speed + crouch * 120, -5 * S(ph) * speed)
    for s, off in (("l", 0), ("r", math.pi)):
        sw = amp * S(ph + off)
        hip, knee = legs_ik(crouch, 0)
        lift = max(0., S(ph + off + 1.2)) * (35 + 25 * speed)
        p[f"thigh_{s}"] = rot(hip - sw)
        p[f"calf_{s}"] = rot(knee + lift)
        p[f"foot_{s}"] = rot(-knee * .3 + sw * .3)
        asw = (18 + 25 * speed) * S(ph + off + math.pi)
        p[f"upperarm_{s}"] = arm(s, asw, 0, 10)
        p[f"lowerarm_{s}"] = rot(-15 - 35 * (speed - 1) * (speed > 1) - 10 * max(0, S(ph + off)))


def idle(t, p):
    base(p)
    b = S(t * TAU)
    p["spine_02"] = rot(2 * b)
    p["neck_01"] = rot(-2 * b, 6 * S(t * TAU * .5))
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, 3 * b, 0, 9 + 2 * b)


def crouch_idle(t, p):
    hip, knee = legs_ik(.13)
    p["pelvis_T"] = (0, -.13, 0)
    p["spine_01"] = rot(22 + 2 * S(t * TAU))
    p["neck_01"] = rot(-18)
    for s in "lr":
        p[f"thigh_{s}"] = rot(hip, 0, 8 if s == "l" else -8)
        p[f"calf_{s}"] = rot(knee)
        p[f"foot_{s}"] = rot(-knee * .45)
        p[f"upperarm_{s}"] = arm(s, -25, 0, 12)
        p[f"lowerarm_{s}"] = rot(-40)


def aim_upper(p, sway=0.):
    # Two hands on a pistol, arms straight out at shoulder height, hands meeting in front.
    p["spine_02"] = rot(4, 0)
    p["spine_03"] = rot(0, sway)
    p["neck_01"] = rot(-6)
    p["upperarm_r"] = arm("r", -82, 18, 2)
    p["upperarm_l"] = arm("l", -78, 30, 4)
    p["lowerarm_r"] = rot(-8)
    p["lowerarm_l"] = rot(-25, 0, 0)
    p["hand_l"] = rot(0, 0, -20)


def aim(t, p):
    idle(t, p)
    aim_upper(p, 1.5 * S(t * TAU))


def aim_walk(t, p):
    walk(t, p, .8)
    aim_upper(p)


def fall(t, p):
    ph = t * TAU * 2
    p["spine_01"] = rot(-8)
    for s, off in (("l", 0), ("r", math.pi)):
        p[f"upperarm_{s}"] = arm(s, -40 + 50 * S(ph + off), 0, 110 + 25 * C(ph + off))
        p[f"lowerarm_{s}"] = rot(-30 - 30 * S(ph + off))
        p[f"thigh_{s}"] = rot(-30 + 30 * S(ph + off))
        p[f"calf_{s}"] = rot(45 + 30 * C(ph + off))


def swim(t, p):
    ph = t * TAU
    p["spine_01"] = rot(15)
    p["neck_01"] = rot(-25)
    for s, off in (("l", 0), ("r", math.pi)):
        p[f"upperarm_{s}"] = arm(s, -60 + 25 * S(ph + off), 25 * C(ph + off), 50)
        p[f"lowerarm_{s}"] = rot(-30)
        p[f"thigh_{s}"] = rot(-15 + 25 * S(2 * ph + off))
        p[f"calf_{s}"] = rot(30 + 20 * S(2 * ph + off + 1))


def seated(t, p, drive=False):
    p["pelvis_T"] = (0, -.08, .05)
    p["spine_01"] = rot(-6 + 1.5 * S(t * TAU))
    for s in "lr":
        p[f"thigh_{s}"] = rot(-88, 0, 6 if s == "l" else -6)
        p[f"calf_{s}"] = rot(85)
        p[f"foot_{s}"] = rot(-5)
        if drive:
            steer = 8 * S(t * TAU)
            p[f"upperarm_{s}"] = arm(s, -55, 15, 15 + (steer if s == "l" else -steer))
            p[f"lowerarm_{s}"] = rot(-45)
        else:
            p[f"upperarm_{s}"] = arm(s, -20, 10, 8)
            p[f"lowerarm_{s}"] = rot(-55)


def drive(t, p):
    seated(t, p, True)


def restrained(t, p, struggle=False):
    walk(t, p, .35) if not struggle else idle(t, p)
    w = 18 * S(t * TAU * 3) if struggle else 0
    p["spine_01"] = rot(8, w)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, 35, -40, 4)
        p[f"lowerarm_{s}"] = rot(-35, 0, 0)
    if struggle:
        p["neck_01"] = rot(-10, -w)


def struggle(t, p):
    restrained(t, p, True)


def carried(t, p):
    p["spine_01"] = rot(35)
    p["spine_02"] = rot(15)
    p["neck_01"] = rot(25 + 5 * S(t * TAU))
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, -30 + 10 * S(t * TAU + (0 if s == "l" else 1)), 0, -5)
        p[f"lowerarm_{s}"] = rot(-5)
        p[f"thigh_{s}"] = rot(-10)
        p[f"calf_{s}"] = rot(20)


def work(t, p):
    hip, knee = legs_ik(.06)
    p["pelvis_T"] = (0, -.06, 0)
    ph = t * TAU * 2
    p["spine_01"] = rot(25 + 6 * S(ph))
    p["neck_01"] = rot(-10)
    for s, off in (("l", 0), ("r", math.pi)):
        p[f"thigh_{s}"] = rot(hip)
        p[f"calf_{s}"] = rot(knee)
        p[f"upperarm_{s}"] = arm(s, -55 + 15 * S(ph + off), 15, 10)
        p[f"lowerarm_{s}"] = rot(-30 - 20 * S(ph + off))


def punch(t, p):
    idle(0, p)
    k = S(min(1., t / .45) * math.pi)  # out and back
    p["spine_02"] = rot(6 * k, -25 * k)
    p["upperarm_r"] = arm("r", -85 * k - 20 * (1 - k), 10, 12)
    p["lowerarm_r"] = rot(-70 * (1 - k) - 5)
    p["upperarm_l"] = arm("l", -40, 25, 10)
    p["lowerarm_l"] = rot(-100)


def fire(t, p):
    aim(0, p)
    k = max(0., 1 - t / .25)
    p["upperarm_r"] = qmul(rot(-12 * k), p["upperarm_r"])
    p["upperarm_l"] = qmul(rot(-12 * k), p["upperarm_l"])


def reload_(t, p):
    idle(0, p)
    k = S(min(1., t / 1.2) * math.pi)
    p["upperarm_r"] = arm("r", -35 - 15 * k, 20, 6)
    p["lowerarm_r"] = rot(-60 - 20 * k)
    p["upperarm_l"] = arm("l", -40 - 10 * k, 35, 6)
    p["lowerarm_l"] = rot(-75 + 15 * S(t * 12))
    p["neck_01"] = rot(15 * k)


def eat(t, p):
    idle(0, p)
    k = S(min(1., t / 1.2) * math.pi)
    p["upperarm_r"] = arm("r", -55 * k, 35 * k, 8)
    p["lowerarm_r"] = rot(-120 * k)
    p["neck_01"] = rot(8 * S(t * 20) * k)


def dance(t, p):
    ph = t * TAU
    p["pelvis_T"] = (.04 * S(ph), -.02 * abs(S(ph)), 0)
    p["pelvis"] = rot(0, 20 * S(ph), 10 * S(ph))
    p["spine_02"] = rot(0, -15 * S(ph), -8 * S(ph))
    for s, off in (("l", 0), ("r", math.pi)):
        p[f"upperarm_{s}"] = arm(s, -30, 0, 120 + 25 * S(2 * ph + off))
        p[f"lowerarm_{s}"] = rot(-40 - 30 * S(2 * ph + off))
        p[f"thigh_{s}"] = rot(-20 * max(0, S(ph + off)))
        p[f"calf_{s}"] = rot(40 * max(0, S(ph + off)))


def trumpet(t, p):
    idle(t, p)
    sway = 8 * S(t * TAU)
    p["spine_02"] = rot(-10, sway)
    p["neck_01"] = rot(-10)
    p["upperarm_r"] = arm("r", -70, 25, 25)
    p["lowerarm_r"] = rot(-110)
    p["upperarm_l"] = arm("l", -75, 30, 10)
    p["lowerarm_l"] = rot(-40)


def wave(t, p):
    idle(t, p)
    p["upperarm_r"] = arm("r", -10, 0, 150)
    p["lowerarm_r"] = rot(0, 0, -30 * S(t * TAU * 2))


def flex(t, p):
    idle(t, p)
    k = .5 + .5 * S(t * TAU)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, -10, 0, 95)
        p[f"lowerarm_{s}"] = rot(-100 - 20 * k)
    p["spine_02"] = rot(-8 * k)


def taunt(t, p):
    idle(t, p)
    p["spine_02"] = rot(10, 20)
    p["upperarm_r"] = arm("r", -80, 0, 10)
    p["lowerarm_r"] = rot(-5)
    p["upperarm_l"] = arm("l", 0, 0, 40)
    p["lowerarm_l"] = rot(-90 - 25 * S(t * TAU * 3))
    p["pelvis"] = rot(0, 0, 6 * S(t * TAU * 2))


def beg(t, p):
    hip, knee = legs_ik(.2)
    p["pelvis_T"] = (0, -.2, 0)
    p["spine_01"] = rot(15)
    p["neck_01"] = rot(-25)
    for s in "lr":
        p[f"thigh_{s}"] = rot(hip)
        p[f"calf_{s}"] = rot(knee)
        p[f"upperarm_{s}"] = arm(s, -60 + 8 * S(t * TAU * 2), 40, 5)
        p[f"lowerarm_{s}"] = rot(-80)


def cheer(t, p):
    idle(t, p)
    b = abs(S(t * TAU * 1.5))
    p["pelvis_T"] = (0, .04 * b, 0)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, -15, 0, 160 + 10 * b)
        p[f"lowerarm_{s}"] = rot(-10 * b)


def climb(t, p):
    ph = t * TAU
    p["spine_01"] = rot(10)
    for s, off in (("l", 0), ("r", math.pi)):
        p[f"upperarm_{s}"] = arm(s, -150 + 25 * S(ph + off), 0, 10)
        p[f"lowerarm_{s}"] = rot(-30 - 30 * max(0, S(ph + off)))
        p[f"thigh_{s}"] = rot(-50 * max(0, S(ph + off + math.pi)))
        p[f"calf_{s}"] = rot(70 * max(0, S(ph + off + math.pi)))


def smoke(t, p):
    """4 s loop: hand up to the mouth, drag, hand down, exhale. Left hand on the hip."""
    idle(t, p)
    c = t % 4.0
    if c < 1.0:
        k = S(c / 1.0 * math.pi / 2)
    elif c < 1.8:
        k = 1.0
    elif c < 2.6:
        k = C((c - 1.8) / .8 * math.pi / 2)
    else:
        k = 0.0
    p["upperarm_r"] = arm("r", -10 - 45 * k, 5 + 30 * k, 12)
    p["lowerarm_r"] = rot(-35 - 90 * k)
    p["upperarm_l"] = arm("l", 15, -10, 35)
    p["lowerarm_l"] = rot(-95)
    exhale = max(0., S((c - 2.6) / 1.4 * math.pi)) if c > 2.6 else 0.
    p["neck_01"] = rot(-12 * exhale + 6 * k)


def kick(t, p):
    idle(0, p)
    k = S(min(1., t / .5) * math.pi)
    p["thigh_r"] = rot(-85 * k)
    p["calf_r"] = rot(40 * (1 - k) * k * 4 if k < .5 else 10 * (1 - k))
    p["spine_01"] = rot(-14 * k)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, 10 * k, 0, 8 + 55 * k)


def headbutt(t, p):
    idle(0, p)
    u = min(1., t / .45)
    k = -S(u * math.pi * 2) * .35 + S(u * math.pi) * (1.0 if u > .3 else 0.)
    p["spine_02"] = rot(28 * k)
    p["neck_01"] = rot(22 * k)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, 20 * max(0, k), 0, 15)


def block(t, p):
    # Boxer's guard: elbows in, forearms up, fists in front of the face, chin tucked, a little bounce.
    idle(t, p)
    b = S(t * TAU) * 2
    p["spine_02"] = rot(10 + b)
    p["neck_01"] = rot(12)
    for s in "lr":
        p[f"upperarm_{s}"] = arm(s, -62, 34, 12)
        p[f"lowerarm_{s}"] = rot(-118)
        p[f"hand_{s}"] = rot(-10)


ANIMS = [
    ("Idle", idle, 2.0, True), ("Walk", lambda t, p: walk(t, p, 1.0), 1.0, True), ("Run", lambda t, p: walk(t, p, 1.8), .62, True),
    ("CrouchIdle", crouch_idle, 2.0, True), ("CrouchWalk", lambda t, p: walk(t, p, .7, .13), 1.1, True),
    ("Fall", fall, 1.0, True), ("Swim", swim, 1.2, True), ("Seated", lambda t, p: seated(t, p), 2.0, True), ("Drive", drive, 2.0, True),
    ("Restrained", lambda t, p: restrained(t, p), 1.6, True), ("Struggle", struggle, .6, True), ("Carried", carried, 2.0, True),
    ("Work", work, 1.0, True), ("Aim", aim, 2.0, True), ("AimWalk", aim_walk, 1.1, True),
    ("Punch", punch, .45, False), ("Fire", fire, .25, False), ("Reload", reload_, 1.2, False), ("Eat", eat, 1.2, False),
    ("Dance", dance, 1.0, True), ("Trumpet", trumpet, 2.0, True), ("Climb", climb, 1.0, True),
    ("Emote_Wave", wave, 1.0, True), ("Emote_Flex", flex, 1.5, True), ("Emote_Taunt", taunt, 1.0, True),
    ("Emote_Beg", beg, 1.0, True), ("Emote_Cheer", cheer, 1.0, True),
    ("Emote_Smoke", smoke, 4.0, True), ("Kick", kick, .5, False), ("Headbutt", headbutt, .45, False),
    ("Block", block, 1.0, True),
]


def sample(fn, t):
    p = {}
    fn(t, p)
    return p


def build():
    g, buf = load(BODY)
    names = {n.get("name"): i for i, n in enumerate(g["nodes"])}
    rest_t = {n.get("name"): np.array(n.get("translation", [0, 0, 0]), float) for n in g["nodes"]}

    def add(arr, typ, minmax=False):
        while len(buf) % 4:
            buf.append(0)
        arr = np.asarray(arr, np.float32)
        off = len(buf)
        buf.extend(arr.tobytes())
        g["bufferViews"].append({"buffer": 0, "byteOffset": off, "byteLength": arr.nbytes})
        a = {"bufferView": len(g["bufferViews"]) - 1, "componentType": 5126, "count": len(arr), "type": typ}
        if minmax:
            a["min"] = [float(arr.min())]
            a["max"] = [float(arr.max())]
        g["accessors"].append(a)
        return len(g["accessors"]) - 1

    g["animations"] = []
    bones = [b for b in names if b and b not in ("SK_PVV_Body",)]
    for anim_name, fn, length, loop in ANIMS:
        frames = max(2, int(round(length * FPS)) + 1)
        times = np.linspace(0, length, frames)
        samples = [sample(fn, (t / length) if loop else t) for t in times]
        if loop:
            samples[-1] = samples[0]
        tacc = add(times.reshape(-1), "SCALAR", True)
        channels, samplers = [], []
        for b in bones:
            if b == "root":
                continue
            q = np.array([s.get(b, np.array([0, 0, 0, 1.])) for s in samples])
            for i in range(1, len(q)):  # keep quaternions on one hemisphere (no flips)
                if np.dot(q[i], q[i - 1]) < 0:
                    q[i] = -q[i]
            samplers.append({"input": tacc, "output": add(q, "VEC4"), "interpolation": "LINEAR"})
            channels.append({"sampler": len(samplers) - 1, "target": {"node": names[b], "path": "rotation"}})
        tr = np.array([rest_t["pelvis"] + np.array(s.get("pelvis_T", (0, 0, 0))) for s in samples])
        samplers.append({"input": tacc, "output": add(tr, "VEC3"), "interpolation": "LINEAR"})
        channels.append({"sampler": len(samplers) - 1, "target": {"node": names["pelvis"], "path": "translation"}})
        g["animations"].append({"name": anim_name, "channels": channels, "samplers": samplers})
    save(OUT, g, buf)
    print("wrote", OUT, len(ANIMS), "animations")


if __name__ == "__main__":
    build()
