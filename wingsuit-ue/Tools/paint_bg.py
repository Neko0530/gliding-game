"""Paints a flat-illustration mountain backdrop (stand-in until Import/background.png is supplied). Pure Python, no deps."""
import math, os, struct, zlib, random

W, H = 1920, 1120
random.seed(7)
img = [bytearray(W * 3) for _ in range(H)]


def put(x, y, c, a=1.0):
    x %= W
    if 0 <= y < H:
        row = img[y]; i = x * 3
        if a >= 1:
            row[i], row[i + 1], row[i + 2] = c
        else:
            row[i] = int(row[i] * (1 - a) + c[0] * a); row[i + 1] = int(row[i + 1] * (1 - a) + c[1] * a); row[i + 2] = int(row[i + 2] * (1 - a) + c[2] * a)


def lerp(a, b, t): return tuple(int(a[k] + (b[k] - a[k]) * t) for k in range(3))


# sky gradient
top, mid, bot = (38, 108, 214), (110, 175, 238), (196, 228, 248)
for y in range(H):
    t = y / (H * 0.75)
    c = lerp(top, mid, t / 0.55) if t < .55 else lerp(mid, bot, min(1, (t - .55) / .45))
    img[y][:] = bytes(c) * W

# light rays
for r in range(5):
    x0 = 700 + r * 170
    for y in range(0, int(H * .62)):
        w = 20 + y * .35
        for x in range(int(x0 - y * .22 - w), int(x0 - y * .22 + w), 2):
            put(x, y, (255, 255, 255), .045)


def ridge(seed, base, amp, f):
    ph = [random.random() * 6.28 for _ in range(4)]
    n1, n2, n3 = {1: (2, 5, 9), 2: (3, 7, 11), 3: (4, 8, 13)}[seed]       # whole numbers of cycles across the width: tiles seamlessly
    return lambda x: base - amp * (.55 * abs(math.sin(math.pi * n1 * x / W + ph[0])) + .3 * abs(math.sin(math.pi * n2 * x / W + ph[1])) + .15 * math.sin(2 * math.pi * n3 * x / W + ph[2]))


layers = [((128, 170, 226), (168, 205, 244), ridge(1, 560, 330, .0040)),
          ((98, 144, 210), (146, 188, 236), ridge(2, 640, 300, .0055)),
          ((70, 116, 188), (112, 158, 220), ridge(3, 720, 230, .0075))]
for dark, light, fn in layers:
    for x in range(W):
        top_y = int(fn(x)); slope = fn(x + 3) - fn(x - 3)
        for y in range(top_y, int(H * .80)):
            lit = slope > 0 and (y - top_y) < 150 + (slope * 6)
            img[y][x * 3:x * 3 + 3] = bytes(light if lit else dark)
        if top_y < 360 and slope < 0:                      # snowy peaks
            for y in range(top_y, top_y + 26):
                put(x, y, (240, 248, 255))

# clouds
def cloud(cx, cy, s):
    blobs = [(0, 0, 1.0), (-1.1, .25, .75), (1.1, .2, .8), (.5, -.55, .8), (-.5, -.5, .7), (1.9, .45, .55), (-1.8, .5, .5)]
    for bx, by, br in blobs:
        R = int(60 * s * br); X = int(cx + bx * 55 * s); Y = int(cy + by * 40 * s)
        for yy in range(-R, R + 1):
            half = int(math.sqrt(max(0, R * R - yy * yy)))
            for xx in range(-half, half + 1):
                shade = (yy + R) / (2 * R)
                put(X + xx, Y + yy, lerp((255, 255, 255), (206, 226, 248), max(0, shade - .45) * 1.6))
for cx, cy, s in [(160, 170, 1.5), (520, 250, 1.3), (980, 210, 1.7), (1500, 150, 1.4), (1780, 300, 1.2), (80, 420, 1.0)]:
    cloud(cx, cy, s)

# rocks along the grass line
gy = int(H * .80)
for k in range(11):
    cx = 40 + k * 175 + random.randint(-25, 25); h = random.randint(70, 140); w = random.randint(110, 170)
    for yy in range(h):
        half = int((w / 2) * (1 - yy / h * .55)) if yy > h * .25 else int(w * .22 + yy * 1.0)
        for xx in range(-half, half):
            put(cx + xx, gy - yy, (150, 150, 190) if xx > -half * .15 else (112, 112, 160))
# grass band with a spiky edge
for x in range(W):
    ed = gy + int(8 * math.sin(2 * math.pi * 29 * x / W) + 6 * math.sin(2 * math.pi * 100 * x / W))
    for y in range(ed - 24, int(H * .90)):
        t = (y - (ed - 24)) / 120
        put(x, y, lerp((120, 214, 52), (58, 150, 34), min(1, t)))
    for k in range(2):
        bh = 28 + int(30 * abs(math.sin(2 * math.pi * 52 * x / W + k)))
        if (x + k * 3) % 8 == 0:
            for j in range(bh):
                put(x + j // 6, ed - 24 - j, (110, 210, 50))
# stone wall
wy = int(H * .90)
for y in range(wy, H):
    for x in range(W):
        img[y][x * 3:x * 3 + 3] = bytes((88, 80, 128))
row = 0
for y in range(wy + 6, H, 54):
    off = 0 if row % 2 == 0 else 60
    for x in range(-off, W, 120):
        for yy in range(y, min(H, y + 48)):
            for xx in range(x + 4, min(W, x + 116)):
                e = min(yy - y, 47 - (yy - y), xx - x - 4, 112 - (xx - x - 4))
                c = (112, 104, 150) if e > 4 else (70, 62, 104)
                put(xx, yy, c)
    row += 1

out = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Import", "background_fallback.png")
os.makedirs(os.path.dirname(out), exist_ok=True)
raw = b"".join(b"\x00" + bytes(r) for r in img)
def chunk(t, d): c = struct.pack(">I", len(d)) + t + d; return c + struct.pack(">I", zlib.crc32(t + d) & 0xffffffff)
png = b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) + chunk(b"IDAT", zlib.compress(raw, 6)) + chunk(b"IEND", b"")
open(out, "wb").write(png)
print("wrote", out, len(png))
