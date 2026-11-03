"""Tileable hair textures (strands), made with numpy - no image files needed.

straight(): fine strands running down the texture (V direction), clumped into locks, with light/dark variation.
curly():    tight coils / kinks (afros, curly tops, twists, waves).
Both return (albedo_png_bytes, normal_png_bytes). The albedo is light grey so the game's hair colour tints it
(colour = texture x Hair Color), the normal map gives every strand a little ridge that catches the light.
"""
import io
import numpy as np
from PIL import Image

N = 512


def _png(arr):
    b = io.BytesIO()
    Image.fromarray(np.clip(arr * 255 + .5, 0, 255).astype(np.uint8)).save(b, "PNG", optimize=True)
    return b.getvalue()


def _normal(H, strength):
    dx = (np.roll(H, -1, 1) - np.roll(H, 1, 1)) * .5 * strength
    dy = (np.roll(H, -1, 0) - np.roll(H, 1, 0)) * .5 * strength
    n = np.stack([-dx, dy, np.ones_like(H)], -1)
    n /= np.linalg.norm(n, axis=-1, keepdims=True)
    return n * .5 + .5


def _blur_x(H, r):
    out = np.zeros_like(H)
    for k in range(-r, r + 1):
        out += np.roll(H, k, 1)
    return out / (2 * r + 1)


def straight(seed=1, strands=2600, wave=2.5):
    rng = np.random.default_rng(seed)
    y = np.arange(N)
    X = np.arange(N)[None, :]
    H = np.zeros((N, N))
    Dark = np.zeros((N, N))
    # Locks: groups of strands share a slow sway, so the hair clumps instead of looking like corduroy.
    locks = 70
    lock_x = rng.uniform(0, N, locks)
    lock_phase = rng.uniform(0, 2 * np.pi, locks)
    lock_m = rng.integers(1, 3, locks)
    for _ in range(strands):
        L = rng.integers(0, locks)
        x0 = (lock_x[L] + rng.normal(0, 7)) % N
        amp = wave * rng.uniform(.4, 1.4)
        xs = x0 + amp * np.sin(2 * np.pi * lock_m[L] * y / N + lock_phase[L]) + rng.uniform(-1, 1) * np.sin(2 * np.pi * 3 * y / N + rng.uniform(0, 6.3))
        w = rng.uniform(.7, 1.6)
        d = (X - xs[:, None] + N / 2) % N - N / 2
        prof = np.exp(-(d / w) ** 2)
        # Each strand fades in and out along its length (so they don't all run the full height).
        fade = .55 + .45 * np.sin(2 * np.pi * rng.integers(1, 4) * y / N + rng.uniform(0, 6.3))
        b = rng.uniform(.4, 1.)
        H += prof * (b * fade)[:, None]
        if rng.random() < .25:
            Dark += prof * fade[:, None]
    H /= H.max()
    clump = _blur_x(H, 6)
    alb = .5 + .38 * H + .12 * clump - .18 * np.clip(Dark / max(Dark.max(), 1e-6), 0, 1)
    alb = np.clip(alb, 0, 1)
    rgb = np.stack([alb, alb * .985, alb * .97], -1)
    return _png(rgb), _png(_normal(H + .6 * clump, 3.2))


def curly(seed=2, coils=5200):
    rng = np.random.default_rng(seed)
    Y, X = np.mgrid[0:N, 0:N].astype(float)
    H = np.zeros((N, N))
    # Dense, overlapping little coils (partial arcs, not full circles), wrapped so the texture tiles.
    for _ in range(coils):
        cx, cy = rng.uniform(0, N, 2)
        R = rng.uniform(2.5, 7.)
        x0, x1 = int(cx - R - 3), int(cx + R + 4)
        y0, y1 = int(cy - R - 3), int(cy + R + 4)
        ys = np.arange(y0, y1) % N
        xs = np.arange(x0, x1) % N
        gy, gx = np.meshgrid(np.arange(y0, y1) - cy, np.arange(x0, x1) - cx, indexing="ij")
        r = np.sqrt(gx ** 2 + gy ** 2)
        ang = np.arctan2(gy, gx)
        a0 = rng.uniform(-np.pi, np.pi)
        arc = np.clip(np.cos((ang - a0) * .5) * 1.6 - .3, 0, 1)  # 2/3 of a ring
        H[np.ix_(ys, xs)] += rng.uniform(.5, 1.) * np.exp(-((r - R) / 1.3) ** 2) * arc
    # Fine frizz on top.
    for _ in range(12):
        fx, fy = rng.integers(20, 70, 2)
        H += .05 * np.abs(np.sin(2 * np.pi * (fx * X + fy * Y) / N + rng.uniform(0, 6.3)))
    H -= H.min()
    H /= np.percentile(H, 99.5)
    H = np.clip(H, 0, 1)
    alb = np.clip(.42 + .5 * H, 0, 1)
    rgb = np.stack([alb, alb * .985, alb * .97], -1)
    return _png(rgb), _png(_normal(H, 3.))


if __name__ == "__main__":
    a, n = straight()
    open("/tmp/hair_straight_albedo.png", "wb").write(a)
    open("/tmp/hair_straight_normal.png", "wb").write(n)
    a, n = curly()
    open("/tmp/hair_curly_albedo.png", "wb").write(a)
    open("/tmp/hair_curly_normal.png", "wb").write(n)
    print("wrote /tmp/hair_*.png")
