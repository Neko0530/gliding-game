"""Synthesises the game's music and sound effects as 16-bit mono WAVs. Pure Python, no dependencies."""
import math, os, random, struct, wave

SR = 22050
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "Import", "Audio")
os.makedirs(OUT, exist_ok=True)
R = random.Random(5)


def save(name, s, peak=.9):
    m = max(1e-9, max(abs(v) for v in s))
    data = b"".join(struct.pack("<h", int(max(-1, min(1, v / m * peak)) * 32767)) for v in s)
    w = wave.open(os.path.join(OUT, name + ".wav"), "wb"); w.setnchannels(1); w.setsampwidth(2); w.setframerate(SR); w.writeframes(data); w.close()
    print(name, len(s) / SR, "s")


def env(n, a, d, sus=0.0, r=0.0):
    out = []
    for i in range(n):
        t = i / SR
        if t < a: v = t / a
        elif t < a + d: v = 1 - (1 - sus) * (t - a) / d
        else: v = sus
        tail = n / SR - r
        if r and t > tail: v *= max(0, 1 - (t - tail) / r)
        out.append(v)
    return out


def lowpass(x, k):
    y = []; p = 0.0
    for v in x: p += k * (v - p); y.append(p)
    return y


def noise(n): return [R.uniform(-1, 1) for _ in range(n)]


def mtof(m): return 440.0 * 2 ** ((m - 69) / 12)


# ---------------- wind loop ----------------
def wind():
    n = SR * 6
    a = lowpass(noise(n), .06); b = lowpass(noise(n), .015)
    s = []
    for i in range(n):
        t = i / SR
        lfo = .65 + .35 * math.sin(2 * math.pi * t / 6 * 1) + .12 * math.sin(2 * math.pi * t / 6 * 3)
        s.append((a[i] * .8 + b[i] * 2.2) * lfo)
    fade = SR // 2                                                  # crossfade the seam so it loops cleanly
    for i in range(fade):
        k = i / fade; s[i] = s[i] * k + s[n - fade + i] * (1 - k)
    save("wind_loop", s[: n - fade], .8)


# ---------------- music: warm adventure loop, 16 bars ----------------
def music():
    bpm = 88; beat = 60 / bpm; bar = beat * 4
    bars = 16; n = int(SR * bar * bars)
    mix = [0.0] * n
    prog = [(48, [60, 64, 67]), (55, [59, 62, 67]), (57, [60, 64, 69]), (53, [60, 65, 69])]       # C  G  Am  F
    scale = [60, 62, 64, 67, 69, 72, 74, 76, 79]                                                   # C major pentatonic+
    def add(start, sig, gain):
        i0 = int(start * SR)
        for j, v in enumerate(sig):
            if i0 + j >= n: break
            mix[i0 + j] += v * gain
    for b in range(bars):
        root, chord = prog[(b // 2) % 4]
        t0 = b * bar
        dur = int(SR * bar)
        e = env(dur, .5, bar - .9, .6, .4)
        # pad: chord tones, slightly detuned pairs
        pad = [0.0] * dur
        for note in chord:
            f = mtof(note)
            for det in (-.4, .4):
                for j in range(dur):
                    pad[j] += math.sin(2 * math.pi * (f + det) * j / SR) * e[j]
        add(t0, pad, .045)
        # bass
        bn = int(SR * beat * 2); be = env(bn, .02, beat * 1.6, .1, .2)
        for k in range(2):
            f = mtof(root)
            add(t0 + k * beat * 2, [math.sin(2 * math.pi * f * j / SR) * be[j] for j in range(bn)], .22)
        # plucked arpeggio (8ths) with a gentle melody
        for k in range(8):
            note = chord[k % 3] + (12 if k % 4 >= 2 else 0)
            if (b % 4 == 3) and k >= 4: note = scale[(k * 3 + b) % len(scale)]
            f = mtof(note); pn = int(SR * beat * 1.1); pe = env(pn, .004, beat * 1.0, 0, .1)
            add(t0 + k * beat / 2, [(math.sin(2 * math.pi * f * j / SR) * .7 + math.sin(4 * math.pi * f * j / SR) * .2) * pe[j] * math.exp(-j / SR * 3.2) for j in range(pn)], .09)
        # melody: long soft notes every other bar
        if b % 2 == 0:
            mel = scale[(b * 2 + 3) % len(scale)]
            f = mtof(mel + 12); mn = int(SR * bar * 1.4); me = env(mn, .12, bar, .5, .5)
            add(t0 + beat, [math.sin(2 * math.pi * (f + 3 * math.sin(j / SR * 5.5)) * j / SR) * me[j] for j in range(mn)], .07)
        # airy hats
        for k in range(8):
            hn = int(SR * .05); he = env(hn, .002, .04)
            add(t0 + k * beat / 2 + beat / 4, [R.uniform(-1, 1) * he[j] for j in range(hn)], .012)
    # soft low-pass to keep it gentle, then loop-safe fade
    mix = lowpass(mix, .45)
    f = SR // 2
    for i in range(f):
        k = i / f; mix[i] = mix[i] * k + mix[n - f + i] * (1 - k) if False else mix[i]
    save("music_loop", mix, .55)


def sfx():
    def bell(f, d, g=1.0):
        n = int(SR * d); e = env(n, .003, d - .003)
        return [(math.sin(2 * math.pi * f * j / SR) + .35 * math.sin(2 * math.pi * f * 2.76 * j / SR)) * e[j] * math.exp(-j / SR * 5) * g for j in range(n)]
    s = bell(880, .7) + [0.0] * 0
    t = bell(1318.5, .9)
    out = [0.0] * int(SR * 1.2)
    for i, v in enumerate(s): out[i] += v
    for i, v in enumerate(t):
        if i + int(SR * .12) < len(out): out[i + int(SR * .12)] += v * .9
    save("gate_chime", out, .8)
    # gate miss: low descending buzz
    n = int(SR * .5); e = env(n, .01, .45)
    save("gate_miss", [math.sin(2 * math.pi * (180 - 70 * j / n) * j / SR) * e[j] * (.6 + .4 * (1 if (j // 90) % 2 else -1)) for j in range(n)], .6)
    # close-pass swoosh
    n = int(SR * .55); ns = lowpass(noise(n), .25); e = env(n, .18, .2, 0, .17)
    save("close_pass", [ns[j] * e[j] * (1 + 2 * math.sin(math.pi * j / n)) for j in range(n)], .7)
    # jump whoosh
    n = int(SR * 1.1); ns = lowpass(noise(n), .12); e = env(n, .3, .5, 0, .3)
    save("jump", [ns[j] * e[j] * (.5 + j / n) for j in range(n)], .8)
    # stall / terrain beep
    n = int(SR * .16); e = env(n, .005, .12)
    save("beep", [(1 if math.sin(2 * math.pi * 880 * j / SR) > 0 else -1) * e[j] * .35 for j in range(n)], .5)
    # crash: low thump + debris noise
    n = int(SR * 1.3); ns = lowpass(noise(n), .18)
    save("crash", [(math.sin(2 * math.pi * (70 - 35 * j / n) * j / SR) * 1.1 + ns[j] * 1.4) * math.exp(-j / SR * 3.2) for j in range(n)], .95)
    # soft landing: thump + short scrape
    n = int(SR * .9); ns = lowpass(noise(n), .3)
    save("land", [(math.sin(2 * math.pi * 90 * j / SR) * math.exp(-j / SR * 9) * 1.0 + ns[j] * math.exp(-j / SR * 2.5) * .45) for j in range(n)], .8)
    # flare air-brake
    n = int(SR * .8); ns = lowpass(noise(n), .2); e = env(n, .1, .35, 0, .35)
    save("flare", [ns[j] * e[j] for j in range(n)], .6)


wind(); music(); sfx()
