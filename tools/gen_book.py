import random, sys
# Offline songwriter: builds many 4-bar loops (verse question/answer + chorus) from motifs, sequences and cadences,
# scores them, and keeps the best ones per progression. Output: C++ data included by Composer.cpp.
OUT = r"D:\Documentos\_Trabajo\DSK Music\_DSK Synths\Memoria by DSK\src\dsp\SongbookData.inc"
B, S, G, J, L, C = 1, 2, 4, 8, 16, 32
PROG44 = [((0,4,5,3),B),((5,3,0,4),S),((0,5,3,4),G),((1,4,0,5),J),((0,3,4,3),B),((5,4,3,4),S|B),((0,3,5,4),B|G),((5,2,3,4),S),
          ((0,2,3,4),B),((3,0,4,5),B),((0,5,1,4),J|L),((0,2,5,3),G|L),((1,4,0,0),J),((0,4,5,2),G|C),((2,3,0,4),S),((5,0,4,3),S|L),
          ((5,3,0,2),C|S),((5,2,3,0),C),((0,3,0,4),G),((3,4,2,5),S|B),((3,4,5,0),B),((0,3,1,4),J),((1,4,5,3),J|L),((5,3,4,0),S),
          ((0,4,3,0),B),((2,5,1,4),J|L),((0,5,4,3),G|L),((3,0,5,4),B|C)]
PROGW = [(0,3,4,0),(5,3,0,4),(0,5,3,4),(0,4,5,3),(0,3,0,4),(5,4,3,4),(0,2,3,4),(5,3,4,0)]
VCELL = {4: [[(0,1.5),(1.5,.5),(2,2)], [(0,1),(1,1),(2,2)], [(0,2),(2,1),(3,1)], [(0,1.5),(1.5,.5),(2,1),(3,1)], [(0,1),(1,.5),(1.5,.5),(2,2)], [(0,2),(2,2)], [(0,1),(1,1),(2,1),(3,1)], [(0,1.5),(1.5,.5),(2,1.5),(3.5,.5)]],
         3: [[(0,2),(2,1)], [(0,1),(1,1),(2,1)], [(0,1.5),(1.5,.5),(2,1)], [(0,1),(1,2)]]}
CCELL = {4: [[(0,.5),(.5,.5),(1,1),(2,2)], [(0,1),(1,1),(2,1.5),(3.5,.5)], [(0,.5),(.5,.5),(1,1),(2,1.5),(3.5,.5)], [(0,1.5),(1.5,.5),(2,1.5),(3.5,.5)], [(0,1),(1,.5),(1.5,.5),(2,1),(3,1)], [(0,2),(2,1),(3,1)], [(0,1),(1,1),(2,2)]],
         3: [[(0,1),(1,1),(2,1)], [(0,2),(2,1)], [(0,1.5),(1.5,.5),(2,1)], [(0,1),(1,.5),(1.5,.5),(2,1)]]}
ENDC = {4: [[(0,1),(1,1),(2,2)], [(0,1.5),(1.5,.5),(2,2)], [(0,2),(2,2)], [(0,2),(2,1),(3,1)], [(0,1),(1,1),(2,1),(3,1)]], 3: [[(0,2),(2,1)], [(0,3)], [(0,1),(1,1),(2,1)]]}

def tones(c): return {c % 7, (c + 2) % 7, (c + 4) % 7}
def nearest(chord, x, lo, hi, prefer=None):
    best = None
    for d in range(lo, hi + 1):
        if d % 7 in tones(chord):
            k = abs(d - x) + (0.01 * (d < x))
            if prefer is not None and d % 7 in prefer: k -= 0.6
            if best is None or k < best[0]: best = (k, d)
    return best[1]
def clampv(x, lo, hi): return max(lo, min(hi, x))

def make_bar(rng, cell, chord, anchor, moves, lo, hi, strongs, final=None):
    notes = []; prev = None
    for i, (b, d) in enumerate(cell):
        strong = (b in strongs) and d >= 0.9
        if i == 0: ideal = anchor
        else: ideal = prev + moves[(i - 1) % len(moves)]
        ideal = clampv(ideal, lo, hi)
        last = i == len(cell) - 1
        if final is not None and last: p = final
        elif strong or i == 0 and b < 0.01: p = nearest(chord, ideal, lo, hi)
        else: p = ideal
        notes.append((b, d, p)); prev = p
    return notes

def fmt(bar):
    return " ".join("%s:%s:%d" % (("%g" % b), ("%g" % d), p) for b, d, p in bar).replace("0.", ".") if False else " ".join("%g:%g:%d" % (b, d, p) for b, d, p in bar)

def phrase(rng, prog, bpb, kind):
    strongs = (0.0,) if bpb == 3 else (0.0, 2.0)
    cells = VCELL[bpb] if kind == 'v' else CCELL[bpb]
    lo, hi = (-1, 7) if kind == 'v' else (4, 12)
    mid = (lo + hi) // 2
    cellA = rng.choice(cells); cellB = rng.choice(cells)
    # motif contour (moves between consecutive notes); mostly steps
    n = max(len(cellA), len(cellB))
    mA = [rng.choice([-2, -1, -1, 1, 1, 2, 0 if rng.random() < .3 else 1]) for _ in range(n)]
    mB = [rng.choice([-2, -1, 1, 1, 2, 2]) for _ in range(n)]
    start = nearest(prog[0], rng.choice([mid - 2, mid - 1, mid, mid + 1]), lo, hi)
    seq = rng.choice([-1, 1, 0, 2])
    bars = []
    b0 = make_bar(rng, cellA, prog[0], start, mA, lo, hi, strongs)
    bars.append(b0)
    # bar 1: the motif again, sequenced (answers its own shape on the new chord)
    s1 = nearest(prog[1], b0[0][2] + seq, lo, hi)
    bars.append(make_bar(rng, cellA, prog[1], s1, mA, lo, hi, strongs))
    # bar 2: contrast, heading to the peak (chorus) or a lift (verse)
    s2 = nearest(prog[2], bars[1][-1][2] + (2 if kind == 'c' else rng.choice([1, 2, -1])), lo, hi)
    bars.append(make_bar(rng, cellB, prog[2], s2, mB if kind == 'c' else [x for x in mB], lo, hi, strongs))
    return bars, lo, hi, strongs

def finish(rng, bars, prog, bpb, kind, answer, lo, hi, strongs):
    last = rng.choice(ENDC[bpb])
    prev = bars[-1][-1][2]
    open_t = {1, 4, 6}; stable = {0, 2, 4}
    fin = nearest(prog[3], prev - rng.choice([1, 2, 3]) if rng.random() < .75 else prev, lo, hi, prefer=(stable if answer else open_t))
    down = [-1, -1, -2, -1, 1]
    bar = make_bar(rng, last, prog[3], nearest(prog[3], prev + rng.choice([-2, -1, -1, 1]), lo, hi), down, lo, hi, strongs, final=fin)
    return bar

def score(verse, chorus, answerV, answerC):
    allb = verse + chorus
    mv = 0; steps = 0; tot = 0; bad = 0; rep = 0
    for bar in allb:
        for i in range(1, len(bar)):
            d = abs(bar[i][2] - bar[i-1][2]); tot += 1
            if d <= 1: steps += 1
            if d > 4: bad += 3
            elif d == 4: bad += 1
            if d == 0: rep += 1
    for k in range(len(allb) - 1):
        d = abs(allb[k+1][0][2] - allb[k][-1][2])
        if d > 4: bad += 3
    if tot == 0: return -99
    sr = steps / tot
    pv = [p for bar in verse for _, _, p in bar]; pc = [p for bar in chorus for _, _, p in bar]
    rng_v = max(pv) - min(pv); rng_c = max(pc) - min(pc)
    s = 10 - 12 * abs(sr - 0.68) - bad - 0.4 * max(0, rng_v - 7) - 0.4 * max(0, rng_c - 7) - 0.7 * max(0, rep - 2)
    s += 0.3 * min(len(set(p % 7 for p in pv)), 6) + 0.3 * min(len(set(p % 7 for p in pc)), 6)
    if sum(pc) / len(pc) - sum(pv) / len(pv) >= 3: s += 2
    peakbar = max(range(4), key=lambda i: max(p for _, _, p in chorus[i]))
    if peakbar in (1, 2): s += 1.5
    if len(set(p for p in pv)) < 4: s -= 3
    return s

def build(prog, bpb, seed):
    rng = random.Random(seed)
    v, lo, hi, st = phrase(rng, prog, bpb, 'v'); c, clo, chi, _ = phrase(rng, prog, bpb, 'c')
    v4 = finish(rng, v, prog, bpb, 'v', False, lo, hi, st); va4 = finish(rng, v, prog, bpb, 'v', True, lo, hi, st)
    c4 = finish(rng, c, prog, bpb, 'c', False, clo, chi, st); ca4 = finish(rng, c, prog, bpb, 'c', True, clo, chi, st)
    return v + [v4], va4, c + [c4], ca4

def validate(prog, bpb, V, vaL, Cc, caL):
    strong = (0.0,) if bpb == 3 else (0.0, 2.0)
    for bars in (V, V[:3] + [vaL], Cc, Cc[:3] + [caL]):
        for i, bar in enumerate(bars):
            tot = max(b + d for b, d, _ in bar)
            if abs(tot - bpb) > 0.01 and abs(tot - bpb) > 0.01: return False
            for b, d, p in bar:
                if b in strong and d >= 0.9 and p % 7 not in tones(prog[i]): return False
    return True

def run(progs, bpb, per, tagfn, seedbase):
    res = []
    for pi, pr in enumerate(progs):
        prog = pr[0] if bpb == 4 else pr
        tag = pr[1] if bpb == 4 else 0
        cands = []
        for k in range(160):
            V, vaL, Cc, caL = build(prog, bpb, seedbase + pi * 1000 + k)
            if not validate(prog, bpb, V, vaL, Cc, caL): continue
            sc = score(V, Cc, vaL, caL)
            sig = (tuple(p for _, _, p in V[0]), tuple(p for _, _, p in Cc[0]))
            cands.append((sc, k, V, vaL, Cc, caL, sig))
        cands.sort(key=lambda x: -x[0])
        picked = []
        for cd in cands:
            if len(picked) >= per: break
            if any(cd[6][0][:3] == q[6][0][:3] or cd[6][1][:3] == q[6][1][:3] for q in picked): continue
            picked.append(cd)
        for cd in picked: res.append((prog, tag, bpb == 3, cd))
    return res


# ---- styles added later (bossa nova, singer-songwriter, tango, village): own rhythm cells, own mood bits, a few hand-written themes
BO, FO, TA, VI = 64, 128, 256, 512
STYLES = {
    BO: dict(progs=[(1,4,0,5),(0,5,1,4),(2,5,1,4),(0,3,1,4),(1,4,0,3),(3,4,0,0)],
             v=[[(0,1.5),(1.5,1),(2.5,1.5)], [(0.5,1),(1.5,.5),(2,1),(3,1)], [(0,.5),(.5,1),(1.5,.5),(2,2)], [(0,1),(1.5,1),(2.5,1.5)], [(0,2),(2.5,1.5)]],
             c=[[(0,1.5),(1.5,1),(2.5,1.5)], [(0,1),(1,.5),(1.5,1),(2.5,1.5)], [(0,.5),(.5,1),(1.5,.5),(2,2)], [(0,1.5),(1.5,.5),(2,2)]]),
    FO: dict(progs=[(0,4,5,3),(0,3,0,4),(5,3,0,4),(0,5,3,4),(0,2,3,4),(3,0,4,0)],
             v=[[(0,1),(1,1),(2,2)], [(0,1.5),(1.5,.5),(2,2)], [(0,1),(1,1),(2,1),(3,1)], [(0,2),(2,1),(3,1)]],
             c=[[(0,1),(1,1),(2,1),(3,1)], [(0,1.5),(1.5,.5),(2,1.5),(3.5,.5)], [(0,1),(1,1),(2,2)], [(0,.5),(.5,.5),(1,1),(2,2)]]),
    TA: dict(progs=[(0,3,4,0),(0,5,3,4),(0,3,4,5),(0,2,3,4),(5,3,4,0),(0,6,3,4)],
             v=[[(0,.75),(.75,.25),(1,1),(2,2)], [(0,1.5),(1.5,.5),(2,2)], [(0,2),(2,1),(3,1)], [(0,.75),(.75,.25),(1,1),(2,.75),(2.75,.25),(3,1)]],
             c=[[(0,.75),(.75,.25),(1,1),(2,.75),(2.75,.25),(3,1)], [(0,1),(1,1),(2,2)], [(0,.5),(.5,.5),(1,1),(2,2)], [(0,1.5),(1.5,.5),(2,1.5),(3.5,.5)]]),
    VI: dict(progs=[(0,3,4,0),(0,4,3,0),(0,5,3,4),(3,0,4,0),(0,3,0,4),(0,2,3,4)],
             v=[[(0,1),(1,.5),(1.5,.5),(2,1),(3,1)], [(0,1),(1,1),(2,1),(3,1)], [(0,.5),(.5,.5),(1,1),(2,2)], [(0,1.5),(1.5,.5),(2,2)]],
             c=[[(0,.5),(.5,.5),(1,1),(2,1),(3,1)], [(0,1),(1,.5),(1.5,.5),(2,2)], [(0,1),(1,1),(2,1.5),(3.5,.5)], [(0,.5),(.5,.5),(1,1),(2,2)]]),
}
HAND = [
    (BO, (1,4,0,5), ["0.5:1:5 1.5:.5:3 2:1:1 3:1:3", "0:1:4 1:.5:6 1.5:1:8 2.5:1.5:6", "0.5:1:7 1.5:.5:4 2:1:2 3:1:4", "0:1.5:5 1.5:1:7 2.5:1.5:9"], "0:1.5:5 1.5:1:4 2.5:1.5:2",
        ["0:1:12 1:.5:10 1.5:1:8 2.5:1.5:10", "0:1.5:11 1.5:.5:13 2:2:11", "0:1:9 1:1:11 2:1:9 3:1:7", "0:2:14 2:2:9"], "0:2:12 2:2:9"),
    (FO, (0,4,5,3), ["0:1:2 1:1:4 2:2:7", "0:1:6 1:1:4 2:2:1", "0:1.5:7 1.5:.5:5 2:2:2", "0:1:5 1:1:3 2:2:5"], "0:1:5 1:1:3 2:2:7",
        ["0:1:4 1:1:7 2:2:9", "0:1:8 1:1:11 2:2:13", "0:1:9 1:1:12 2:2:14", "0:1.5:10 1.5:.5:12 2:2:14"], "0:1.5:12 1.5:.5:10 2:2:7"),
    (TA, (0,3,4,0), ["0:.75:4 .75:.25:2 1:1:0 2:2:2", "0:.75:5 .75:.25:3 1:1:5 2:2:7", "0:1:6 1:1:4 2:1:6 3:1:8", "0:2:4 2:2:4"], "0:2:2 2:2:0",
        ["0:.5:7 .5:.5:7 1:1:9 2:2:11", "0:.75:12 .75:.25:10 1:1:12 2:2:14", "0:1:13 1:1:11 2:1:13 3:1:8", "0:2:11 2:2:9"], "0:2:9 2:2:7"),
    (VI, (0,3,4,0), ["0:1:2 1:1:4 2:1:2 3:1:4", "0:1:5 1:1:3 2:1:5 3:1:7", "0:1:6 1:1:4 2:1:6 3:1:4", "0:2:4 2:1:2 3:1:4"], "0:2:2 2:2:0",
        ["0:.5:7 .5:.5:9 1:1:11 2:2:9", "0:.5:10 .5:.5:12 1:1:14 2:2:12", "0:.5:11 .5:.5:13 1:1:11 2:2:13", "0:2:11 2:2:9"], "0:1:11 1:1:9 2:2:7"),
]
def parsebar(t):
    out = []
    for tok in t.split():
        b, d, p = tok.split(':'); out.append((float('0' + b if b.startswith('.') else b), float('0' + d if d.startswith('.') else d), int(p)))
    return out
def extra_items():
    res = []
    global VCELL, CCELL
    saveV, saveC = VCELL[4], CCELL[4]
    for tag, d in STYLES.items():
        VCELL[4], CCELL[4] = d['v'], d['c']
        for pi, prog in enumerate(d['progs']):
            cands = []
            for k in range(160):
                V, vaL, Cc, caL = build(prog, 4, 500000 + tag * 100 + pi * 1000 + k)
                if not validate(prog, 4, V, vaL, Cc, caL): continue
                sc = score(V, Cc, vaL, caL)
                sig = (tuple(p for _, _, p in V[0]), tuple(p for _, _, p in Cc[0]))
                cands.append((sc, k, V, vaL, Cc, caL, sig))
            cands.sort(key=lambda x: -x[0])
            picked = []
            for cd in cands:
                if len(picked) >= 5: break
                if any(cd[6][0][:3] == q[6][0][:3] or cd[6][1][:3] == q[6][1][:3] for q in picked): continue
                picked.append(cd)
            for cd in picked: res.append((prog, tag, False, cd))
    VCELL[4], CCELL[4] = saveV, saveC
    for tag, prog, v, va, c, ca in HAND:
        V = [parsebar(x) for x in v]; vaL = parsebar(va); Cc = [parsebar(x) for x in c]; caL = parsebar(ca)
        ok = validate(prog, 4, V, vaL, Cc, caL)
        print("hand", tag, "valid" if ok else "INVALID")
        if not ok: sys.exit(1)
        res.append((prog, tag, False, (99, 0, V, vaL, Cc, caL, None)))
    return res

if __name__ == "__main__":
    items = run(PROG44, 4, 5, None, 100) + run(PROGW, 3, 5, None, 90000) + extra_items()
    print(len(items), "loops")
    lines = ["// generated by gen_book.py: do not edit by hand\n", "struct GenLoop { int ch[4]; int moods; bool waltz; const char* s[10]; };\n", "static const GenLoop kGen[] {\n"]
    for prog, tag, w, (sc, k, V, vaL, Cc, caL, sig) in items:
        strs = [fmt(b) for b in V] + [fmt(vaL)] + [fmt(b) for b in Cc] + [fmt(caL)]
        lines.append('  { { %d, %d, %d, %d }, %d, %s, { %s } },\n' % (prog + (tag if not w else 0, "true" if w else "false", ", ".join('"%s"' % x for x in strs))))
    lines.append("};\n")
    open(OUT, "w", encoding="utf-8").write("".join(lines))
    avg = sum(i[3][0] for i in items) / len(items)
    print("avg score %.2f" % avg)
