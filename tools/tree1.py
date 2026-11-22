import os
sp = os.path.dirname(os.path.abspath(__file__))
p = os.path.join(sp, 'room3d.glsl')
s = open(p, encoding='utf-8').read().replace('\r\n', '\n')
def R(o, n):
    global s
    assert o in s, o[:80]
    s = s.replace(o, n, 1)

helpers = '''float hillY (float x) { return 0.22 + 0.05 * sin (x * 8.0) + 0.03 * sin (x * 21.0 + 1.0); }
float segB (vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp (dot (pa, ba) / dot (ba, ba), 0.0, 1.0);
    return length (pa - ba * h);
}
// one clump of foliage: x = coverage (with a ragged, leafy edge and gaps), y = shading (lit from the upper left)
vec2 clumpD (vec2 q, float cx, float cy, float r, float sd)
{
    vec2 d = (q - vec2 (cx, cy)) / r;
    float n = fbm (q * 52.0 + sd) - 0.5;
    float dist = length (d) + n * 0.60;
    float cover = ss (1.0, 0.70, dist) * ss (0.26, 0.50, fbm (q * 95.0 + sd * 3.0) + 0.45 * uFoAmt);
    float shade = 0.34 + 0.90 * clamp (dot (d, vec2 (-0.45, 0.85)) * 0.5 + 0.5, 0.0, 1.0);
    return vec2 (cover, shade);
}

'''
R('vec3 skyColour (vec2 w)', helpers + 'vec3 skyColour (vec2 w)')

a = s.index('    float crownE = crown + 0.07')
e = s.index('    return outc * outK;\n}\n') + len('    return outc * outK;\n}\n')
new = '''    bool isTrunkOld = abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12;
    if (uSeason < 0.5)
    {
        if (isTrunkOld || w.y < hill || crown < 0.0) c = dark;
        return c * 1.25;
    }
    c *= mix (1.0, 0.60, uSeason);
    vec3 lightC = mix (vec3 (1.0), uBot * 1.25, 0.5) * (0.20 + 0.65 * uOutL);
    vec3 outc = c;
    // a distant row of small trees on the far hill, hazy
    for (int k = 0; k < 4; k++)
    {
        float fk = float (k), fx = 0.60 + 0.11 * fk + 0.02 * hash12 (vec2 (fk, 1.0));
        vec2 fq = w - vec2 (fx, hillY (fx) + 0.035);
        float fcov = ss (1.0, 0.65, length (fq / vec2 (0.040, 0.052)) + 0.5 * (fbm (w * 55.0 + fk * 7.0) - 0.5)) * mix (0.30, 1.0, uFoAmt);
        vec3 fcol = mix (uFoliage * lightC * (0.55 + 0.35 * fk * 0.3), c, 0.45);
        outc = mix (outc, fcol, fcov);
    }
    // ground: grass (or snow) with fine streaks, hazier towards the horizon, and the soft shadow of the tree
    if (w.y < hill)
    {
        float far = ss (hill - 0.07, hill, w.y);
        vec3 gcol = uGround * lightC * (0.74 + 0.40 * fbm (w * vec2 (34.0, 150.0)) + 0.18 * (fbm (w * vec2 (160.0, 40.0)) - 0.5));
        gcol *= 0.80 + 0.35 * ss (0.0, hill, w.y);
        gcol *= 1.0 - 0.30 * ss (0.17, 0.0, length ((w - vec2 (0.31, hill - 0.02)) * vec2 (1.0, 3.6)));
        outc = mix (gcol, c * 0.9, 0.20 * far + 0.10);
    }
    // the tree: tapered trunk, branches, clumps of leaves (none in winter: bare branches, snow on them)
    vec2 q = w - vec2 (0.25, 0.20);
    float swayT = 0.006 * sin (uTime * 1.5 + w.y * 6.0) * uWind;
    float ty = clamp (q.y / 0.19, 0.0, 1.0);
    float tx = q.x - 0.008 * sin (ty * 2.5) - swayT * ty;
    float trunkW = mix (0.017, 0.0065, ty);
    float trunk = ss (trunkW, trunkW * 0.55, abs (tx)) * step (0.0, q.y) * step (q.y, 0.20);
    vec2 qb = q - vec2 (swayT * 2.0, 0.0);
    float br = max (max (ss (0.0060, 0.0030, segB (qb, vec2 (0.0, 0.14), vec2 (-0.075, 0.245))), ss (0.0060, 0.0030, segB (qb, vec2 (0.0, 0.15), vec2 (0.080, 0.255)))),
                    max (ss (0.0052, 0.0026, segB (qb, vec2 (0.0, 0.17), vec2 (0.004, 0.31))), max (ss (0.0032, 0.0016, segB (qb, vec2 (-0.052, 0.215), vec2 (-0.11, 0.30))), ss (0.0032, 0.0016, segB (qb, vec2 (0.057, 0.222), vec2 (0.115, 0.30))))));
    vec3 bark = vec3 (0.20, 0.14, 0.10) * lightC * (0.75 + 0.5 * fbm (vec2 (q.x * 160.0, q.y * 14.0)));
    outc = mix (outc, bark, max (trunk, br * 0.9));
    outc = mix (outc, vec3 (0.93, 0.95, 1.0) * lightC, uSnow * br * ss (0.30, 0.6, fbm (q * 70.0)) * 0.55);
    if (uFoAmt > 0.02)
    {
        vec2 qc = q - vec2 (swayT * 3.0, 0.0);
        vec2 best = vec2 (0.0, 0.5);
        float kk = 0.0;
        for (int i = 0; i < 8; i++)
        {
            float fi = float (i);
            float cx = i == 0 ? -0.075 : i == 1 ? 0.0 : i == 2 ? 0.078 : i == 3 ? -0.105 : i == 4 ? 0.110 : i == 5 ? 0.004 : i == 6 ? -0.038 : 0.045;
            float cy = i == 0 ? 0.265 : i == 1 ? 0.305 : i == 2 ? 0.268 : i == 3 ? 0.225 : i == 4 ? 0.230 : i == 5 ? 0.235 : i == 6 ? 0.338 : 0.345;
            float cr = i == 0 ? 0.056 : i == 1 ? 0.066 : i == 2 ? 0.056 : i == 3 ? 0.040 : i == 4 ? 0.042 : i == 5 ? 0.052 : i == 6 ? 0.045 : 0.040;
            vec2 cd = clumpD (qc, cx, cy, cr, fi * 7.3);
            if (cd.x > best.x * 0.9) { kk = fi; }
            if (cd.x > best.x) best = cd;
        }
        vec3 tone2 = uFoliage * vec3 (1.28, 1.02, 0.66) + vec3 (0.02, 0.02, 0.0);
        vec3 fol = mix (uFoliage, tone2, hash12 (vec2 (kk, 3.0)) * 0.7) * lightC * best.y * (0.78 + 0.5 * fbm (q * 130.0));
        fol += uBlossom * step (0.70, fbm (q * 75.0 + 7.0)) * vec3 (0.96, 0.74, 0.80) * lightC * 0.9 * best.y;
        outc = mix (outc, fol, clamp (best.x * uFoAmt * 1.15, 0.0, 1.0));
    }
    return outc * 0.72;
}
'''
s = s[:a] + new + s[e:]
# the old silhouette needs its own trunk test and no noisy crown
R('    bool isTrunk = abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12;\n    bool isGround = w.y < hill;\n', '') if '    bool isTrunk = abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12;\n    bool isGround = w.y < hill;\n' in s else None
open(p, 'w', encoding='utf-8', newline='').write(s)
print('ok')
