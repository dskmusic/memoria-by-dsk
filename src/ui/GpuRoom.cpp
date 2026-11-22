#include "GpuRoom.h"

namespace gx::ui
{
using namespace juce::gl;

namespace
{
const char* vertexShader = R"GLSL(
attribute vec2 position;
void main() { gl_Position = vec4 (position, 0.0, 1.0); }
)GLSL";

// ------------------------------------------------------------------ the room
const char* roomShader = R"GLSL(
uniform vec2 uRes;
uniform vec3 uTop, uBot, uSunCol, uGround, uFoliage;
uniform float uSunI, uSunPx, uSunPy, uDirX, uDirY, uAmb, uNight, uCloud, uWind, uRain, uOutL, uFoAmt, uSnow, uBlossom, uLeaf, uSeason, uHours, uSeed, uSection;
uniform float uTime, uMemory, uAge, uWorld, uEnergy, uHit, uPedal, uFlame, uIntimacy, uWarmth, uSpace;
uniform float uKeys[88];
#ifdef MINI
const int MARCH = 40;
const int FOGN = 1;
#elif defined(LITE)
const int MARCH = 56;
const int FOGN = 6;
#else
const int MARCH = 90;
const int FOGN = 16;
#endif

float hash12 (vec2 p) { vec3 p3 = fract (vec3 (p.xyx) * 0.1031); p3 += dot (p3, p3.yzx + 33.33); return fract ((p3.x + p3.y) * p3.z); }
float hash13 (vec3 p3) { p3 = fract (p3 * 0.1031); p3 += dot (p3, p3.zyx + 31.32); return fract ((p3.x + p3.y) * p3.z); }
vec2 hash22 (vec2 p) { vec3 p3 = fract (vec3 (p.xyx) * vec3 (0.1031, 0.1030, 0.0973)); p3 += dot (p3, p3.yzx + 33.33); return fract ((p3.xx + p3.yz) * p3.zy); }
float valueNoise2 (vec2 p)
{
    vec2 i = floor (p), f = fract (p);
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (hash12 (i), hash12 (i + vec2 (1.0, 0.0)), f.x), mix (hash12 (i + vec2 (0.0, 1.0)), hash12 (i + vec2 (1.0, 1.0)), f.x), f.y);
}
float fbm (vec2 p)
{
    float v = 0.0, a = 0.5;
    mat2 m = mat2 (1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 5; i++) { v += a * valueNoise2 (p); p = m * p; a *= 0.5; }
    return v;
}
float valueNoise3 (vec3 p)
{
    vec3 i = floor (p), f = fract (p);
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (mix (hash13 (i), hash13 (i + vec3 (1, 0, 0)), f.x), mix (hash13 (i + vec3 (0, 1, 0)), hash13 (i + vec3 (1, 1, 0)), f.x), f.y),
                mix (mix (hash13 (i + vec3 (0, 0, 1)), hash13 (i + vec3 (1, 0, 1)), f.x), mix (hash13 (i + vec3 (0, 1, 1)), hash13 (i + vec3 (1, 1, 1)), f.x), f.y), f.z);
}

float ss (float a, float b, float x) { float t = clamp ((x - a) / (b - a), 0.0, 1.0); return t * t * (3.0 - 2.0 * t); }

float sdBox (vec3 p, vec3 b) { vec3 d = abs (p) - b; return length (max (d, 0.0)) + min (max (d.x, max (d.y, d.z)), 0.0); }
float sdRoundBox (vec3 p, vec3 b, float r) { return sdBox (p, b - vec3 (r)) - r; }
float sdCyl (vec3 p, float h, float r) { vec2 d = abs (vec2 (length (p.xz), p.y)) - vec2 (r, h); return min (max (d.x, d.y), 0.0) + length (max (d, 0.0)); }
vec2 opU (vec2 a, vec2 b) { return a.x < b.x ? a : b; }

// room: x from -3.5 (window wall) to 3.6, z from -3 (back wall) to the camera, y from 0 to 3.2
const float WY0 = 0.95, WY1 = 2.55, WX0 = -2.55, WX1 = -0.65;
const vec3 PIANO = vec3 (1.35, 0.0, -2.62);
const vec3 FLAMEP = vec3 (1.95, 1.40, -2.62);
vec3 SUNL;                                          // direction towards the light (through the window in the back wall); set in main from the hour
const vec3 CLK = vec3 (0.20, 2.38, -2.99);          // the wall clock

vec2 map (vec3 p)
{
    vec2 r = vec2 (p.y, 1.0);
    r = opU (r, vec2 (3.2 - p.y, 12.0));
    r = opU (r, vec2 (3.6 - p.x, 2.0));
    r = opU (r, vec2 (p.x + 3.5, 2.0));
    vec2 wc = vec2 (0.5 * (WX0 + WX1), 0.5 * (WY0 + WY1)), wh = vec2 (0.5 * (WX1 - WX0), 0.5 * (WY1 - WY0));
    vec2 q2 = abs (p.xy - wc) - wh;
    float dOpen = length (max (q2, 0.0)) + min (max (q2.x, q2.y), 0.0);
    r = opU (r, vec2 (max (p.z + 3.0, -dOpen), 2.0));
    r = opU (r, vec2 (p.z + 3.15, 20.0));
    vec3 wq = p - vec3 (wc.x, wc.y, -3.0);
    float dFrame = max (sdBox (wq, vec3 (wh.x + 0.06, wh.y + 0.06, 0.07)), -sdBox (wq, vec3 (wh.x - 0.035, wh.y - 0.035, 1.0)));
    float dMull = min (sdBox (wq, vec3 (0.016, wh.y, 0.035)), sdBox (vec3 (wq.x, abs (wq.y) - 0.27, wq.z), vec3 (wh.x, 0.013, 0.035)));
    r = opU (r, vec2 (min (dFrame, dMull), 11.0));
    r = opU (r, vec2 (sdBox (p - vec3 (wc.x, WY0 - 0.03, -2.88), vec3 (wh.x + 0.1, 0.022, 0.15)), 11.0));
    float fold = 0.018 * sin (p.x * 34.0 + p.y * 1.5) + 0.01 * sin (p.x * 71.0);
    float sway = uWind * 0.07 + 0.045 * clamp (uEnergy * 4.0 + uSection, 0.0, 1.0);
    fold += sway * (0.5 + 0.5 * sin (uTime * 0.31)) * (1.9 - p.y * 0.5) * sin (uTime * 1.3 + p.y * 1.8 + p.x * 3.0);
    r = opU (r, vec2 (sdBox (p - vec3 (WX0 - 0.22, 1.62, -2.86), vec3 (0.2 + fold, 1.55, 0.12)), 8.0));
    r = opU (r, vec2 (sdBox (p - vec3 (WX1 + 0.22, 1.62, -2.86), vec3 (0.2 + fold, 1.55, 0.12)), 8.0));
    // wall clock: brass bezel and enamel dial (the hands are painted on the dial in material())
    vec3 cq = p - CLK;
    if (abs (cq.x) < 0.36 && abs (cq.y) < 0.36 && cq.z < 0.3)
    {
        float cd = length (cq.xy);
        r = opU (r, vec2 (max (abs (cd - 0.2625) - 0.0275, abs (cq.z - 0.03) - 0.045), 14.0));
        r = opU (r, vec2 (max (cd - 0.236, abs (cq.z - 0.02) - 0.018), 15.0));
        r = opU (r, vec2 (max (cd - 0.31, abs (cq.z - 0.0) - 0.012), 11.0));
    }
    // picture on the back wall
    r = opU (r, vec2 (sdBox (p - vec3 (1.35, 1.95, -2.975), vec3 (0.42, 0.30, 0.02)), 13.0));
    // the piano (upright) and its bench, only evaluated near them
    vec3 q = p - PIANO;
    if (abs (q.x - 0.0) < 1.1 && q.y < 1.6 && q.z > -0.5 && q.z < 1.2)
    {
        float body = sdRoundBox (q - vec3 (0.0, 0.62, 0.0), vec3 (0.78, 0.52, 0.32), 0.035);
        r = opU (r, vec2 (body, 3.0));
        r = opU (r, vec2 (sdRoundBox (q - vec3 (0.0, 1.17, 0.02), vec3 (0.82, 0.024, 0.35), 0.012), 3.0));
        r = opU (r, vec2 (sdBox (q - vec3 (0.0, 0.07, 0.0), vec3 (0.76, 0.07, 0.34)), 6.0));
        // keyboard slab and cheek blocks
        r = opU (r, vec2 (sdBox (q - vec3 (0.0, 0.725, 0.41), vec3 (0.66, 0.02, 0.13)), 4.0));
        r = opU (r, vec2 (sdRoundBox (q - vec3 (-0.69, 0.76, 0.41), vec3 (0.045, 0.07, 0.14), 0.01), 3.0));
        r = opU (r, vec2 (sdRoundBox (q - vec3 (0.69, 0.76, 0.41), vec3 (0.045, 0.07, 0.14), 0.01), 3.0));
        // music desk with a sheet, tilted back
        vec3 md = q - vec3 (0.0, 1.0, 0.3);
        float ca = cos (0.32), sa = sin (0.32);
        md = vec3 (md.x, ca * md.y + sa * md.z, -sa * md.y + ca * md.z);
        r = opU (r, vec2 (sdBox (md, vec3 (0.46, 0.18, 0.012)), 10.0));
        r = opU (r, vec2 (sdBox (md - vec3 (0.0, -0.17, 0.02), vec3 (0.46, 0.012, 0.03)), 6.0));
        // pedals and a lyre
        r = opU (r, vec2 (sdBox (q - vec3 (-0.12, 0.05, 0.3), vec3 (0.035, 0.008, 0.05)), 14.0));
        r = opU (r, vec2 (sdBox (q - vec3 (0.12, 0.05, 0.3), vec3 (0.035, 0.008, 0.05)), 14.0));
        // candle on the piano
        r = opU (r, vec2 (sdCyl (q - vec3 (0.6, 1.26, 0.0), 0.012, 0.045), 14.0));
        r = opU (r, vec2 (sdCyl (q - vec3 (0.6, 1.325, 0.0), 0.05, 0.0165), 9.0));
    }
    // bench
    vec3 bq = p - vec3 (1.3, 0.0, -1.78);
    if (abs (bq.x) < 0.6 && abs (bq.z) < 0.4 && p.y < 0.7)
    {
        r = opU (r, vec2 (sdRoundBox (bq - vec3 (0.0, 0.50, 0.0), vec3 (0.34, 0.045, 0.16), 0.03), 6.0));
        r = opU (r, vec2 (sdRoundBox (bq - vec3 (0.0, 0.55, 0.0), vec3 (0.31, 0.03, 0.13), 0.025), 8.0));
        vec3 lq = vec3 (abs (bq.x) - 0.29, bq.y - 0.23, abs (bq.z) - 0.12);
        r = opU (r, vec2 (sdBox (lq, vec3 (0.022, 0.23, 0.022)), 6.0));
    }
    // a rug
    r = opU (r, vec2 (max (p.y - 0.012, sdBox (p - vec3 (0.9, 0.0, -1.4), vec3 (1.6, 0.02, 1.5))), 7.0));
    return r;
}

vec3 calcNormal (vec3 p)
{
#ifdef LITE
    return normalize (cross (dFdx (p), dFdy (p)));
#else
    vec3 n = vec3 (0.0);
    for (int i = 0; i < 4; i++)
    {
        vec3 e = i == 0 ? vec3 (1.0, -1.0, -1.0) : i == 1 ? vec3 (-1.0, -1.0, 1.0) : i == 2 ? vec3 (-1.0, 1.0, -1.0) : vec3 (1.0, 1.0, 1.0);
        n += e * map (p + e * 0.0015).x;
    }
    return normalize (n);
#endif
}

float softShadow (vec3 ro, vec3 rd, float tmax)
{
#ifdef LITE
    return 1.0;
#endif
    float res = 1.0, t = 0.03 + 0.03 * hash12 (gl_FragCoord.xy);
    for (int i = 0; i < 26; i++)
    {
        float h = map (ro + rd * t).x;
        res = min (res, 9.0 * h / t);
        t += clamp (h, 0.03, 0.4);
        if (res < 0.01 || t > tmax) break;
    }
    return clamp (res, 0.0, 1.0);
}

float candleShadow (vec3 ro, vec3 rd, float tmax)
{
#ifdef LITE
    return 1.0;
#endif
    float res = 1.0;
    float t = 0.02 + 0.03 * hash12 (gl_FragCoord.xy + 3.7);
    for (int i = 0; i < 40; i++)
    {
        float h = map (ro + rd * t).x;
        res = min (res, 5.0 * h / t);
        t += clamp (h * 0.8, 0.012, 0.18);
        if (res < 0.005 || t > tmax - 0.03) break;
    }
    res = clamp (res, 0.0, 1.0);
    return res * res * (3.0 - 2.0 * res);
}

float calcAO (vec3 p, vec3 n)
{
#ifdef LITE
    return 1.0;
#endif
    float occ = 0.0, sca = 1.0;
    for (int i = 0; i < 4; i++)
    {
        float h = 0.02 + 0.13 * float (i);
        occ += (h - map (p + n * h).x) * sca;
        sca *= 0.75;
    }
    return clamp (1.0 - 2.2 * occ, 0.0, 1.0);
}

// how much of the window the point sees along the light direction (mullions cut it into panes)
float windowLight (vec3 q)
{
    float t = (-3.0 - q.z) / SUNL.z;
    if (t < 0.0) return 0.0;
    vec3 h = q + SUNL * t;
    vec2 wc = vec2 (0.5 * (WX0 + WX1), 0.5 * (WY0 + WY1)), wh = vec2 (0.5 * (WX1 - WX0), 0.5 * (WY1 - WY0));
    vec2 d = abs (h.xy - wc) - wh;
    float inside = ss (0.05, -0.05, max (d.x, d.y));
    float mull = max (ss (0.04, 0.012, abs (h.x - wc.x)), ss (0.03, 0.01, abs (abs (h.y - wc.y) - 0.27)));
    return inside * (1.0 - 0.8 * mull);
}

float hillY (float x) { return 0.22 + 0.05 * sin (x * 8.0) + 0.03 * sin (x * 21.0 + 1.0); }
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
    float n = fbm (q * 70.0 + sd) - 0.5;
    float dist = length (d) + n * 0.60;
    float cover = ss (1.0, 0.62, dist) * ss (0.24, 0.50, fbm (q * 150.0 + sd * 3.0) + 0.42 * uFoAmt);
    float shade = 0.34 + 0.90 * clamp (dot (d, vec2 (-0.45, 0.85)) * 0.5 + 0.5, 0.0, 1.0);
    return vec2 (cover, shade);
}

vec3 skyColour (vec2 w)
{
    vec2 sunP = vec2 (uSunPx, uSunPy);
    vec3 sg = uSunCol / max (max (uSunCol.r, max (uSunCol.g, uSunCol.b)), 0.001);
    vec3 c = mix (uBot, uTop, pow (clamp (w.y, 0.0, 1.0), 0.6));
    float d = length ((w - sunP) * vec2 (1.0, 1.2));
    c += sg * uSunI * (exp (-d * 6.0) * 0.7 + ss (0.045, 0.03, d) * 1.2);
    c += vec3 (0.75, 0.82, 1.0) * uNight * (exp (-d * 7.0) * 0.30 + ss (0.04, 0.028, d) * 0.85);    // the moon
    float cl = fbm (vec2 (w.x * 3.0 + uTime * (0.01 + 0.08 * uWind), w.y * 6.0));
    c = mix (c, c * 0.7 + vec3 (0.15), ss (0.5, 0.8, cl) * uCloud);
    if (uNight > 0.01)
    {
        vec2 g = w * 44.0, gid = floor (g);
        vec2 gf = fract (g) - 0.5 - (vec2 (hash12 (gid + 1.3), hash12 (gid + 2.9)) - 0.5) * 0.5;
        c += step (0.985, hash12 (gid)) * ss (0.20, 0.0, length (gf)) * vec3 (0.7, 0.75, 0.9) * (0.5 + 0.5 * sin (uTime * 2.0 + hash12 (gid) * 30.0)) * uNight;
    }
    float hill = 0.22 + 0.05 * sin (w.x * 8.0) + 0.03 * sin (w.x * 21.0 + 1.0);
    vec3 dark = c * 0.09 + vec3 (0.0, 0.004, 0.01);
    vec2 t = w - vec2 (0.25, 0.34);
    float crown = length (t * vec2 (2.2, 1.0)) - 0.14;
    bool isTrunkOld = abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12;
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
)GLSL" R"GLSL(
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
    vec2 q = (w - vec2 (0.25, 0.20)) / 0.80;
    float swayT = 0.0;      // the tree does not move with the wind (it looked odd)
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
    // bare twigs in winter: a fine, dark lattice inside the crown, with a little snow on it
    float canopy = ss (1.0, 0.55, length ((q - vec2 (0.0, 0.27)) / vec2 (0.17, 0.11)));
    float twig = step (0.60, fbm (q * 52.0 + 3.0)) * canopy * (1.0 - uFoAmt);
    outc = mix (outc, bark, twig * 0.8);
    outc = mix (outc, vec3 (0.93, 0.95, 1.0) * lightC, uSnow * twig * ss (0.35, 0.6, fbm (q * 90.0)) * 0.5);
    if (uFoAmt > 0.02 && abs (q.x) < 0.24 && q.y > 0.1 && q.y < 0.46)
    {
        vec2 qc = q - vec2 (swayT * 3.0, 0.0);
        vec2 best = vec2 (0.0, 0.5);
        float kk = 0.0;
        for (int i = 0; i < 16; i++)
        {
            float fi = float (i);
            float cx = i == 0 ? -0.075 : i == 1 ? 0.0 : i == 2 ? 0.078 : i == 3 ? -0.105 : i == 4 ? 0.110 : i == 5 ? 0.004 : i == 6 ? -0.038 : i == 7 ? 0.045 : 0.115 * cos (fi * 2.399) * (0.6 + 0.4 * hash12 (vec2 (fi, 2.0)));
            float cy = i == 0 ? 0.265 : i == 1 ? 0.305 : i == 2 ? 0.268 : i == 3 ? 0.225 : i == 4 ? 0.230 : i == 5 ? 0.235 : i == 6 ? 0.338 : i == 7 ? 0.345 : 0.275 + 0.085 * sin (fi * 2.399) * (0.6 + 0.4 * hash12 (vec2 (fi, 5.0)));
            float cr = i == 0 ? 0.056 : i == 1 ? 0.066 : i == 2 ? 0.056 : i == 3 ? 0.040 : i == 4 ? 0.042 : i == 5 ? 0.052 : i == 6 ? 0.045 : i == 7 ? 0.040 : 0.026 + 0.012 * hash12 (vec2 (fi, 7.0));
            vec2 cd = clumpD (qc, cx, cy, cr, fi * 7.3);
            if (cd.x > best.x * 0.9) { kk = fi; }
            if (cd.x > best.x) best = cd;
        }
        vec3 tone2 = uFoliage * vec3 (1.28, 1.02, 0.66) + vec3 (0.02, 0.02, 0.0);
        vec3 fol = mix (uFoliage, tone2, hash12 (vec2 (kk, 3.0)) * 0.7) * lightC * best.y * (0.70 + 0.65 * fbm (q * 190.0));
        fol += uBlossom * step (0.70, fbm (q * 90.0 + 7.0)) * vec3 (0.96, 0.74, 0.80) * lightC * 0.9 * best.y;
        outc = mix (outc, fol, clamp (best.x * uFoAmt * 1.15, 0.0, 1.0));
    }
    return outc * 0.72;
}

vec4 fallers (vec2 uv)
{
    vec4 r = vec4 (0.0);
    float amt = max (uSnow, max (uLeaf, uBlossom * 0.6));
    if (amt < 0.01) return r;
    for (int k = 0; k < 3; k++)
    {
        float fk = float (k), s = 9.0 + 7.0 * fk;
        vec2 g = uv * vec2 (s * 0.8, s) + vec2 (fk * 7.3, fk * 2.9);
        g.y += uTime * (0.35 + 0.18 * fk) * (uSnow > 0.5 ? 1.0 : 0.6);
        g.x += sin (uTime * 0.5 + fk * 3.0 + g.y * 0.6) * 0.5;
        vec2 id = floor (g), f = fract (g) - 0.5;
        float h = hash12 (id + fk * 5.1);
        if (h > amt * 0.5) continue;
        vec2 off = (vec2 (hash12 (id + 1.7), hash12 (id + 8.3)) - 0.5) * 0.5;
        vec2 sc2 = uSnow > 0.5 ? vec2 (1.0, 1.0) : vec2 (1.0, 1.9);
        float d = length ((f - off) * sc2);
        float rad = (0.08 + 0.05 * hash12 (id + 3.3)) * (1.0 - 0.18 * fk);
        float a = clamp (exp (-d * d / (rad * rad * 0.35)) * (0.60 + 0.15 * fk) - 0.05, 0.0, 1.0);
        vec3 col = uSnow > 0.5 ? vec3 (0.95, 0.96, 1.0) : (uLeaf > 0.5 ? mix (vec3 (0.85, 0.42, 0.10), vec3 (0.62, 0.20, 0.06), hash12 (id + 9.9)) : vec3 (0.97, 0.78, 0.82));
        r.rgb = mix (r.rgb, col, a); r.a = max (r.a, a);
    }
    return r;
}

vec3 rainDrops (vec2 uv, float t)
{
    vec3 r = vec3 (0.0);
    for (int k = 0; k < 3; k++)
    {
        float fk = float (k), s = 7.0 + 6.0 * fk;
        vec2 g = uv * vec2 (s * 0.75, s) + vec2 (fk * 7.3, fk * 2.1);
        vec2 id = floor (g), f = fract (g) - 0.5;
        float rn = hash12 (id + fk * 13.7), rn2 = hash12 (id + 5.1 + fk);
        vec2 pos = vec2 ((rn - 0.5) * 0.5, 0.38 - 0.76 * fract (t * (0.08 + 0.35 * rn2) + rn * 9.0));
        float rad = 0.05 + 0.06 * rn2, d = length (f - pos);
        float drop = ss (rad, rad * 0.35, d);
        float trail = ss (0.03, 0.0, abs (f.x - pos.x)) * step (pos.y, f.y) * ss (pos.y + 0.4, pos.y, f.y) * 0.35;
        float on = step (0.35, rn2);
        r.xy += (f - pos) * drop * 0.9 * on;
        r.z += (drop * 0.9 + trail * 1.4) * on;
    }
    return r;
}

vec3 windowSky (vec3 p)
{
    vec2 wc = vec2 (0.5 * (WX0 + WX1), 0.5 * (WY0 + WY1)), wh = vec2 (0.5 * (WX1 - WX0), 0.5 * (WY1 - WY0));
    vec2 wu = vec2 ((p.x - (wc.x - wh.x)) / (2.0 * wh.x), (p.y - (wc.y - wh.y)) / (2.0 * wh.y));
    vec3 rd = uRain > 0.5 ? rainDrops (wu * vec2 (1.3, 1.0), uTime) : vec3 (0.0);
    vec3 sc = skyColour (wu + rd.xy * 0.06);
    if (uRain > 0.5) sc = sc * (0.80 + rd.z * 0.45) + rd.z * 0.22;
    vec4 fl = fallers (wu);
    sc = mix (sc, fl.rgb * (0.30 + 0.9 * uOutL), fl.a * 0.9);
    float edge = min (min (wu.x, 1.0 - wu.x), min (wu.y, 1.0 - wu.y) * 1.6);
    float frost = uSnow * ss (0.17, 0.0, edge + 0.08 * fbm (wu * 16.0));
    sc = mix (sc, vec3 (0.82, 0.88, 0.95) * (0.45 + 0.55 * uOutL), frost * 0.5);
    return sc;
}

vec3 sunColour() { return uSunCol; }

// key index (0..87) under a point of the keyboard, and which key it is
float keyDown (float midiIdx)
{
    float v = 0.0;
#ifdef LITE
    return v;
#endif
    for (int i = 0; i < 88; i++) if (float (i) == midiIdx) v = uKeys[i];
    return v;
}

// a soft, faded photograph that changes with the preset (seed): horizon, a couple of figures out of focus, grain
vec3 photo (vec2 u)
{
    float s = fract (uSeed * 0.6180339), s2 = fract (uSeed * 0.3819 + 0.31);
    float hz = -0.25 + 0.5 * s;
    float tone = 0.62 + 0.25 * fbm (u * 2.0 + s * 9.0);
    vec3 sky = mix (vec3 (0.80, 0.74, 0.62), vec3 (0.55, 0.52, 0.48), ss (hz, 1.0, u.y));
    vec3 gnd = mix (vec3 (0.36, 0.31, 0.24), vec3 (0.20, 0.17, 0.13), ss (hz, -1.0, u.y));
    vec3 c = mix (gnd, sky, ss (hz - 0.06, hz + 0.06, u.y));
    float bx = -0.5 + s2;
    float fig = ss (0.30, 0.0, length ((u - vec2 (bx, hz + 0.12)) * vec2 (2.4, 0.95)) - 0.22);
    float fig2 = ss (0.26, 0.0, length ((u - vec2 (bx + 0.30, hz + 0.05)) * vec2 (2.8, 1.1)) - 0.16);
    c = mix (c, vec3 (0.10, 0.085, 0.07), max (fig, fig2) * 0.85);
    c *= tone + 0.2;
    c = mix (c, vec3 (dot (c, vec3 (0.33))), 0.55);
    c *= mix (vec3 (1.08, 0.97, 0.80), vec3 (0.92, 1.0, 1.08), step (0.78, s));
    c *= 1.0 - 0.45 * dot (u, u) * 0.5;
    c += (fbm (u * 90.0) - 0.5) * 0.12;
    return c;
}

float segD (vec2 p, vec2 a, vec2 b)
{
    vec2 pa = p - a, ba = b - a;
    float h = clamp (dot (pa, ba) / dot (ba, ba), 0.0, 1.0);
    return length (pa - ba * h);
}

// the dial of the clock: ticks, hands (with a soft shadow), seconds hand and hub; the time comes from uHours
vec3 clockDial (vec2 cp)
{
    float cr = length (cp), an = atan (cp.x, cp.y);
    vec3 face = vec3 (0.88, 0.84, 0.72) * (0.93 + 0.07 * fbm (cp * 40.0));
    face *= 1.0 - 0.18 * ss (0.14, 0.24, cr);
    vec3 ink = vec3 (0.07, 0.06, 0.055);
    float pxm = 6.2831853 * cr;
    float fm = abs (fract (an * 60.0 / 6.2831853 + 0.5) - 0.5) * pxm / 60.0;
    float fh = abs (fract (an * 12.0 / 6.2831853 + 0.5) - 0.5) * pxm / 12.0;
    float mt = ss (0.0018, 0.0008, fm) * step (0.205, cr) * step (cr, 0.225);
    float ht = ss (0.0042, 0.0028, fh) * step (0.182, cr) * step (cr, 0.226);
    float ring = ss (0.0016, 0.0006, abs (cr - 0.229)) + ss (0.0012, 0.0004, abs (cr - 0.1985));
    face = mix (face, ink, clamp (max (max (mt, ht), ring * 0.8), 0.0, 1.0));
    float hh = mod (uHours, 12.0);
    float aH = hh / 12.0 * 6.2831853, aM = fract (hh) * 6.2831853, aS = floor (mod (uHours * 3600.0, 60.0)) / 60.0 * 6.2831853;
    vec2 dH = vec2 (sin (aH), cos (aH)), dM = vec2 (sin (aM), cos (aM)), dS = vec2 (sin (aS), cos (aS));
    vec2 so = vec2 (0.007, -0.010);
    float shadow = max (ss (0.010, 0.002, segD (cp - so, -dH * 0.03, dH * 0.125)), ss (0.007, 0.001, segD (cp - so, -dM * 0.035, dM * 0.19)));
    face *= 1.0 - 0.38 * shadow;
    float hand = max (ss (0.0075, 0.0055, segD (cp, -dH * 0.03, dH * 0.125)), ss (0.0050, 0.0033, segD (cp, -dM * 0.035, dM * 0.19)));
    face = mix (face, ink, hand);
    float sec = ss (0.0024, 0.0013, segD (cp, -dS * 0.05, dS * 0.215)) + ss (0.011, 0.008, length (cp - dS * 0.17) - 0.0);
    face = mix (face, vec3 (0.55, 0.07, 0.05), clamp (sec * step (0.0, 1.0) * 0.9, 0.0, 1.0) * (1.0 - hand * 0.0));
    face = mix (face, vec3 (0.75, 0.55, 0.22), ss (0.012, 0.009, cr));
    return face;
}

// material at a point: albedo, roughness, base reflectivity
void material (vec3 p, vec3 n, float id, out vec3 alb, out float rough, out float f0, out float bump)
{
    alb = vec3 (0.5); rough = 0.6; f0 = 0.04; bump = 0.0;
    if (id < 1.5)       // floor: boards along x
    {
        float board = floor (p.z / 0.16);
        float shade = 0.75 + 0.5 * hash12 (vec2 (board, 3.0));
        float grain = fbm (vec2 (p.x * 2.5 + hash12 (vec2 (board, 1.0)) * 40.0, p.z * 90.0));
        float seam = ss (0.012, 0.0, abs (fract (p.z / 0.16) - 0.5) - 0.485) + ss (0.01, 0.0, abs (fract ((p.x + hash12 (vec2 (board, 7.0)) * 3.0) / 1.2) - 0.5) - 0.495);
        alb = vec3 (0.34, 0.20, 0.11) * shade * (0.7 + 0.6 * grain) * (1.0 - 0.6 * min (seam, 1.0));
        rough = 0.30; f0 = 0.05; bump = grain;
    }
    else if (id < 2.5)  // plaster wall with a wainscot
    {
        alb = vec3 (0.46, 0.34, 0.26) * (0.88 + 0.24 * fbm (p.xy * 4.0));
        if (n.z > 0.5 && p.y < 1.05) { alb = vec3 (0.20, 0.13, 0.09) * (0.8 + 0.4 * fbm (vec2 (p.x * 3.0, p.y * 20.0))); if (abs (p.y - 1.05) < 0.03) alb = vec3 (0.30, 0.20, 0.13); }
        rough = 0.9;
    }
    else if (id < 3.5)  // black lacquer
    {
        alb = vec3 (0.012, 0.010, 0.011) + 0.006 * fbm (p.xy * 40.0);
        rough = 0.08; f0 = 0.06;
    }
    else if (id < 5.5)  // keys
    {
        float wx = (p.x - PIANO.x + 0.66) / 1.32 * 52.0;
        float wi = floor (wx), wf = fract (wx);
        float m = mod (wi, 7.0);
        float semis = (m < 0.5 ? 0.0 : m < 1.5 ? 2.0 : m < 2.5 ? 3.0 : m < 3.5 ? 5.0 : m < 4.5 ? 7.0 : m < 5.5 ? 8.0 : 10.0) + 12.0 * floor (wi / 7.0);
        float midiIdx = semis;
        float pressed = keyDown (midiIdx);
        alb = mix (vec3 (0.80, 0.76, 0.66), vec3 (0.97, 0.94, 0.86), hash12 (vec2 (wi, 4.0))) * (1.0 - 0.12 * ss (0.0, 0.06, wf < 0.5 ? wf : 1.0 - wf) * 0.0);
        alb *= 1.0 - 0.55 * ss (0.03, 0.0, min (wf, 1.0 - wf));          // the gap between keys
        float zf = (p.z - (PIANO.z + 0.41)) ;
        bool hasBlack = m < 1.5 || (m > 2.5 && m < 5.5);
        float bx = abs (wf - 1.0) < 0.02 ? 0.0 : 0.0;
)GLSL" R"GLSL(
        // black keys sit between white keys: on the right edge of white keys with a sharp (A#, C#, D#, F#, G#)
        float edge = (wf > 0.68 && hasBlack && m < 6.5 && zf < 0.0) ? 1.0 : 0.0;
        float edgeL = (wf < 0.32 && (m > 0.5 && m < 1.5 + 0.0 || (m > 3.5 && m < 6.5) || (m > 2.5 && m < 3.5)) && zf < 0.0) ? 0.0 : 0.0;
        if (edge > 0.5 || (n.y < 0.5 && id > 4.5)) alb = vec3 (0.012);
        alb = mix (alb, alb * vec3 (1.1, 0.85, 0.6), pressed * 0.5);
        rough = 0.25; f0 = 0.04;
        bump = pressed;
    }
    else if (id < 6.5)  // wood
    {
        float g = fbm (vec2 (p.x * 6.0 + p.y * 2.0, p.z * 40.0 + p.y * 40.0));
        alb = vec3 (0.20, 0.11, 0.06) * (0.6 + 0.8 * g); rough = 0.35; f0 = 0.05;
    }
    else if (id < 7.5)  // rug
    {
        vec2 u = p.xz * 9.0;
        float pat = 0.5 + 0.5 * sin (u.x) * sin (u.y);
        float border = ss (0.15, 0.0, min (min (abs (p.x - 0.7) - 1.35, 0.0) * -1.0, min (abs (p.z + 1.5) - 1.15, 0.0) * -1.0) - 0.0);
        alb = mix (vec3 (0.30, 0.06, 0.05), vec3 (0.55, 0.32, 0.16), pat) * (0.55 + 0.5 * fbm (p.xz * 60.0));
        rough = 1.0; f0 = 0.0;
    }
    else if (id < 8.5)  // curtain / cushion
    {
        float fo = 0.55 + 0.45 * sin (p.x * 34.0 + p.y * 1.5);
        alb = vec3 (0.42, 0.09, 0.07) * (0.5 + 0.6 * fo) * (0.8 + 0.4 * fbm (p.xy * 30.0)); rough = 1.0; f0 = 0.0;
    }
    else if (id < 9.5)  { alb = vec3 (0.92, 0.85, 0.68); rough = 0.5; }     // candle wax
    else if (id < 10.5) // sheet music
    {
        vec3 q = p - PIANO - vec3 (0.0, 1.0, 0.3);
        float l = step (0.5, fract (q.y * 65.0)) * step (abs (q.x), 0.40);
        alb = vec3 (0.80, 0.75, 0.62) * (1.0 - 0.18 * l * step (0.0, q.y + 0.1)); rough = 0.8;
    }
    else if (id < 11.5) { alb = vec3 (0.16, 0.10, 0.07); rough = 0.45; }
    else if (id < 12.5) { alb = vec3 (0.50, 0.42, 0.34); rough = 1.0; }          // ceiling
    else if (id < 13.5) // picture
    {
        vec2 u = p.xy - vec2 (1.35, 1.95);
        float inside = step (abs (u.x), 0.37) * step (abs (u.y), 0.255);
        float mat = step (abs (u.x), 0.34) * step (abs (u.y), 0.225);
        vec3 art = photo (u / vec2 (0.34, 0.225));
        alb = mix (vec3 (0.28, 0.17, 0.08), mix (vec3 (0.80, 0.76, 0.66), art, mat), inside); rough = 0.5;
    }
    else if (id < 14.5) { alb = vec3 (0.75, 0.55, 0.22); rough = 0.25; f0 = 0.6; }                // brass
    else { alb = clockDial (p.xy - CLK.xy); rough = 0.22; f0 = 0.05; }                              // the clock dial
}

vec3 environment (vec3 r, float rough)
{
    // what a glossy surface mirrors: the window, the lamp-lit wall, the dark room
    vec3 sky = mix (vec3 (0.05, 0.035, 0.03), vec3 (0.20, 0.14, 0.10), clamp (r.y * 0.5 + 0.5, 0.0, 1.0));
    return sky * (1.0 - 0.5 * rough);
}

vec3 shadeHit (vec3 ro, vec3 rd, float t, float id)
{
    vec3 p = ro + rd * t;
    int wd = int (uWorld + 0.5);
    if (id > 19.5) return windowSky (p);
    vec3 n = calcNormal (p);
    if (dot (n, rd) > 0.0) n = -n;
    vec3 alb; float rough, f0, bump;
    material (p, n, id, alb, rough, f0, bump);
    if (id > 7.5 && id < 8.5) n = normalize (n + vec3 (0.5 * cos (p.x * 34.0 + p.y * 1.5), 0.0, 0.0) * 0.4);
    float ao = calcAO (p, n);
    vec3 sc = sunColour();
    float flick = uFlame * (0.94 + 0.06 * sin (uTime * 11.0)) + 0.16 * uEnergy + 0.10 * uHit + 0.12 * uSection;
    // the window: sun through the glass with soft shadows, plus sky fill from the opening
    float wl = windowLight (p + n * 0.02);
    float sunD = max (dot (n, SUNL), 0.0);
    float sh = wl > 0.01 && sunD > 0.0 ? softShadow (p + n * 0.02, SUNL, 7.0) : 0.0;
    vec3 col = vec3 (0.0);
    col += alb * sc * sunD * sh * wl * 2.2;
    vec3 wcen = vec3 (-1.6, 1.75, -2.9);
    vec3 toW = wcen - p; float dW = length (toW); toW /= dW;
    float fill = max (dot (n, toW), 0.0) * 1.8 / (1.0 + 0.35 * dW * dW);
    vec3 skyFill = mix (vec3 (0.9, 0.8, 0.7), sc, 0.4) * uAmb;
    col += alb * skyFill * fill * ao * 1.1;
    // bounce / ambient
    col += alb * mix (vec3 (0.07, 0.045, 0.03), vec3 (0.15, 0.12, 0.10), n.y * 0.5 + 0.5) * ao * (1.0 - 0.5 * uNight);
    // the candle
    vec3 pl = FLAMEP + vec3 (0.002 * sin (uTime * 6.0), 0.0, 0.0) - p;
    float dc = length (pl); pl /= dc;
    float cs = dot (n, pl) > 0.0 ? candleShadow (p + n * 0.02, pl, dc) : 0.0;
    vec3 candle = vec3 (1.0, 0.56, 0.22) * flick * (1.0 + 0.6 * uNight);
    col += alb * candle * max (dot (n, pl), 0.0) * cs * (1.6 / (1.0 + 7.0 * dc * dc)) * ao;
    // reflections: the window and the candle in glossy surfaces
    vec3 r = reflect (rd, n);
    float fres = f0 + (1.0 - f0) * pow (1.0 - max (dot (n, -rd), 0.0), 5.0);
    float gloss = 1.0 - rough;
    if (gloss > 0.1)
    {
        float tw = (-3.15 - p.z) / r.z;
        vec3 env = environment (r, rough);
        if (tw > 0.0)
        {
            vec3 hp = p + r * tw;
            vec2 wc = vec2 (0.5 * (WX0 + WX1), 0.5 * (WY0 + WY1)), wh = vec2 (0.5 * (WX1 - WX0), 0.5 * (WY1 - WY0));
            vec2 d = abs (hp.xy - wc) - wh;
            float inW = ss (0.06 + rough * 0.4, -0.04, max (d.x, d.y));
            env += windowSky (hp) * inW * 1.6;
        }
        vec3 cp = FLAMEP - p;
        env += candle * pow (max (dot (r, normalize (cp)), 0.0), 60.0 + 600.0 * gloss * gloss) * 1.2 / (1.0 + 3.0 * dot (cp, cp));
        env += sc * pow (max (dot (r, SUNL), 0.0), 30.0 + 300.0 * gloss * gloss) * wl * sh * 1.4;
        col += env * fres * ao * (id > 2.5 && id < 3.5 ? 1.5 : 1.0);
    }
    col += alb * vec3 (1.0, 0.6, 0.25) * bump * (id > 3.5 && id < 5.5 ? 0.25 : 0.0);
    if (id > 8.5 && id < 9.5) col += vec3 (1.0, 0.6, 0.25) * 0.15 * flick;
    // dim the far end, a little haze
    return col * exp (-t * 0.02);
}

float fogLight (vec3 ro, vec3 rd, float tmax)
{
    float acc = 0.0;
#ifdef MINI
    return 0.0;
#endif
    float j = hash12 (gl_FragCoord.xy + fract (uTime) * 17.0);
    for (int i = 0; i < FOGN; i++)
    {
        float t = (float (i) + j) / float (FOGN) * min (tmax, 8.0);
        vec3 q = ro + rd * t;
        float vis = windowLight (q);
        if (q.x < -3.45 || q.x > 3.55 || q.y < 0.0 || q.y > 3.2 || q.z < -2.95) continue;
        float dust = 0.6 + 0.9 * valueNoise3 (q * 2.2 + vec3 (uTime * 0.03, uTime * -0.02, 0.0));
        acc += vis * dust;
    }
    float ph = 0.35 + 1.4 * pow (max (dot (rd, -SUNL), 0.0), 3.0);
    return acc / float (FOGN) * min (tmax, 8.0) * ph;
}

vec3 mote (vec3 ro, vec3 rd)
{
    vec3 add = vec3 (0.0);
#ifdef LITE
    return add;
#endif
    for (int l = 0; l < 3; l++)
    {
        float fl = float (l);
        float z = 2.0 + 1.6 * fl;
        vec3 q = ro + rd * z;
        vec3 g = q * (9.0 + 4.0 * fl) + vec3 (uTime * 0.05, uTime * -0.04 + uHit * 0.2 * fl, fl * 3.7);
        vec3 id = floor (g), f = fract (g) - 0.5;
        float h = hash13 (id + fl);
        if (h < 0.82) continue;
        vec3 off = vec3 (hash13 (id + 3.1), hash13 (id + 7.7), hash13 (id + 9.3)) - 0.5;
        float d = length (f - off * 0.6);
        float vis = windowLight (q);
        add += sunColour() * ss (0.12, 0.0, d) * vis * (0.6 + 0.4 * sin (uTime * (1.0 + 3.0 * h) + h * 40.0)) * 1.6;
    }
    return add;
}

vec3 sceneColour (vec2 uv0)
{
    float asp = uRes.x / uRes.y;
    vec2 p2 = (uv0 - 0.5) * vec2 (asp, 1.0);
    // a slow dolly and a drift of the camera
    vec3 ro = vec3 (0.35 + 0.10 * sin (uTime * 0.045), 1.30 + 0.025 * sin (uTime * 0.07), 3.55 - 0.18 * sin (uTime * 0.035 + 1.0));
    vec3 target = vec3 (0.15 + 0.05 * sin (uTime * 0.05), 1.10, -1.8);
    vec3 f = normalize (target - ro), rt = normalize (cross (f, vec3 (0.0, 1.0, 0.0))), up = cross (rt, f);
    vec3 rd = normalize (p2.x * rt + p2.y * up + 1.75 * f);
    float t = 0.05; vec2 h = vec2 (0.0);
    float hitId = -1.0;
    for (int i = 0; i < MARCH; i++)
    {
        h = map (ro + rd * t);
        if (h.x < 0.0012 * t) { hitId = h.y; break; }
        t += h.x * 0.9;
        if (t > 14.0) break;
    }
    vec3 col = vec3 (0.02);
    if (hitId > 0.0) col = shadeHit (ro, rd, t, hitId); else t = 14.0;
    // light shafts, dust, flame
    vec3 sc = sunColour();
    col += sc * fogLight (ro, rd, t) * 0.16 * (0.9 + 0.5 * uEnergy);
    col += mote (ro, rd);
    vec3 fv = FLAMEP + vec3 (0.002 * sin (uTime * 6.0), 0.0, 0.0);
    vec3 toF = fv - ro; float tF = dot (toF, rd);
    if (tF > 0.0 && tF < t + 0.05)
    {
        float dF = length (toF - rd * tF);
        float flick = uFlame * (0.94 + 0.06 * sin (uTime * 11.0)) + 0.16 * uEnergy + 0.10 * uHit + 0.12 * uSection;
        float shape = dF / (0.011 * (1.0 + 0.2 * sin (uTime * 9.0)));
        col += mix (vec3 (1.0, 0.55, 0.15), vec3 (1.0, 0.95, 0.8), exp (-shape * shape * 0.8)) * exp (-shape * shape * 0.25) * 2.2 * flick;
        col += vec3 (1.0, 0.55, 0.22) * exp (-dF * 22.0 / (0.6 + tF * 0.2)) * 0.30 * flick;
    }
    return col;
}

vec3 aces (vec3 x) { return clamp ((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }

void main()
{
    SUNL = normalize (vec3 (uDirX, uDirY, -0.85));
    vec2 uv0 = gl_FragCoord.xy / uRes;
    float aging = clamp (uMemory * (0.55 + 0.45 * uAge), 0.0, 1.0);
    vec2 uv = uv0;
    if (aging > 0.3)
    {
        uv.y += (hash12 (vec2 (floor (uTime * 14.0), 2.0)) - 0.5) * 0.004 * aging;
        uv.x += 0.0012 * aging * sin (uv0.y * 60.0 + uTime * 3.0) * (0.5 + 0.5 * sin (uTime * 0.6));
    }
    vec3 c = sceneColour (uv);
    c = aces (c * 1.55);
    float lum = dot (c, vec3 (0.3, 0.59, 0.11));
    c = mix (c, vec3 (lum * 1.14, lum * 0.93, lum * 0.66), 0.82 * aging);
    c += vec3 (0.05, 0.043, 0.032) * aging;
    vec2 v = (uv0 - 0.5) * vec2 (1.0, 1.1);
    c *= 1.0 - (0.30 + 0.45 * aging) * pow (clamp (length (v) * 1.3, 0.0, 1.0), 2.2);
    c += vec3 (1.0, 0.45, 0.18) * ss (0.82, 1.0, uv0.x) * (0.12 + 0.08 * sin (uTime * 0.7)) * ss (0.4, 0.9, aging);
    float sc = 0.0;
    for (int i = 0; i < 3; i++)
    {
        float fi = float (i);
        float sx = hash12 (vec2 (floor (uTime * 9.0), fi * 3.1));
        sc += ss (0.0007, 0.0, abs (uv0.x - sx)) * step (0.35, hash12 (vec2 (fi, floor (uTime * 9.0) + 2.0)));
    }
    c += vec3 (0.20, 0.18, 0.14) * sc * ss (0.25, 0.7, aging);
    {
        float fr = floor (uTime * 12.0);
        vec2 gc = uv0 * uRes / 16.0 + vec2 (fr * 7.3, fr * 3.1);
        vec2 id = floor (gc);
        float on = step (0.992, hash12 (id));
        vec2 f = fract (gc) - 0.5 - (hash22 (id + 4.7) - 0.5) * 0.45;
        float rad = 0.10 + 0.26 * hash12 (id + 9.1);
        float wob = valueNoise3 (vec3 (f * 7.0, hash12 (id) * 30.0)) * 0.30;
        float d = length (f * vec2 (1.0, 0.7 + 0.7 * hash12 (id + 2.2))) - rad * (0.75 + wob);
        float speck = ss (0.05, -0.015, d) * on;
        c = mix (c, vec3 (0.03, 0.025, 0.02), speck * ss (0.25, 0.7, aging) * 0.75);
        float on2 = step (0.9965, hash12 (id + 17.0));
        c = mix (c, c + vec3 (0.10, 0.09, 0.07), ss (0.06, -0.01, length (f * vec2 (1.0, 1.4)) - rad * 0.8 * (0.7 + wob)) * on2 * ss (0.25, 0.7, aging));
    }
    c += (hash12 (uv0 * uRes + fract (uTime * 7.3) * 113.0) - 0.5) * (0.014 + 0.07 * aging);
    gl_FragColor = vec4 (clamp (c, 0.0, 1.0), 1.0);
}

)GLSL";

const char* bloomShader = R"GLSL(
uniform sampler2D uTex;
uniform vec2 uRes;
uniform float uAmt;
void main()
{
    vec2 uv = gl_FragCoord.xy / uRes;
    vec3 c = texture2D (uTex, uv).rgb;
    vec3 b = vec3 (0.0), w = vec3 (0.0);
    vec2 asp = vec2 (uRes.y / uRes.x, 1.0);
    for (int i = 0; i < 32; i++)
    {
        float fi = float (i);
        float r = sqrt ((fi + 0.5) / 32.0);
        float a = fi * 2.39996;
        vec2 o = vec2 (cos (a), sin (a)) * asp;
        vec3 s1 = texture2D (uTex, uv + o * r * 0.016).rgb;
        vec3 s2 = texture2D (uTex, uv + o * r * 0.06).rgb;
        b += max (s1 - 0.55, 0.0) * (1.0 - 0.5 * r);
        w += max (s2 - 0.6, 0.0) * (1.0 - 0.7 * r);
    }
    gl_FragColor = vec4 (c + (b * 0.10 + w * 0.06) * uAmt, 1.0);
}
)GLSL";
}

// ------------------------------------------------------------------ GpuRoom
namespace
{
juce::CriticalSection infoLock;
juce::String infoText;
}
juce::String getGpuInfo() { const juce::ScopedLock sl (infoLock); return infoText; }
void setGpuInfo (const juce::String& t) { const juce::ScopedLock sl (infoLock); infoText = t; }

bool GpuRoom::init (juce::OpenGLContext& c)
{
    ctx = &c;
    error.clear();
    const juce::String renderer = juce::String ((const char*) glGetString (GL_VERSION)) + "  " + juce::String ((const char*) glGetString (GL_RENDERER));
    auto log = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DSK Music").getChildFile ("Memoria").getChildFile ("gpu-log.txt");
    // two tiers: the full scene, then a lighter one (no soft shadows / ambient occlusion / dust) if the driver rejects or cannot link the first
    for (int tier = gpuForceTier() >= 0 ? gpuForceTier() : 0; tier < 3 && prog == nullptr; ++tier)
    {
        auto p = std::make_unique<juce::OpenGLShaderProgram> (c);
        const juce::String fs = (tier == 1 ? juce::String ("#define LITE 1\n") : tier == 2 ? juce::String ("#define LITE 1\n#define MINI 1\n") : juce::String()) + roomShader;
        if (p->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader))
            && p->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fs))
            && p->link())
        {
            prog = std::move (p);
            setGpuInfo (juce::String (tier == 0 ? "OK (full) - " : tier == 1 ? "OK (light) - " : "OK (minimal) - ") + renderer);
            log.getParentDirectory().createDirectory();
            log.replaceWithText ("Memoria GPU scene\nOpenGL: " + renderer + "\nTier: " + juce::String (tier) + "\n" + error);
        }
        else error << (tier == 0 ? "full: " : tier == 1 ? "light: " : "minimal: ") << p->getLastError() << "\n";
    }
    if (prog == nullptr)
    {
        DBG (error);
        setGpuInfo ("shader failed - " + renderer + " - " + error.replaceCharacters ("\r\n", "  ").substring (0, 420));
        log.getParentDirectory().createDirectory();
        log.replaceWithText ("Memoria GPU scene\nOpenGL: " + renderer + "\n\n" + error);
        return false;
    }
    {
        auto b = std::make_unique<juce::OpenGLShaderProgram> (c);
        if (b->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader))
            && b->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (bloomShader)) && b->link())
            bloom = std::move (b);
        else error << "bloom: " << b->getLastError() << "\n";
    }
    const GLfloat quad[] { -1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f };
    glGenBuffers (1, &vbo);
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glBufferData (GL_ARRAY_BUFFER, sizeof (quad), quad, GL_STATIC_DRAW);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
    return true;
}

void GpuRoom::release()
{
    prog.reset(); bloom.reset();
    fb.release(); fbW = fbH = 0;
    if (vbo != 0 && ctx != nullptr) glDeleteBuffers (1, &vbo);
    vbo = 0;
}

void GpuRoom::drawQuad (juce::OpenGLShaderProgram& p)
{
    const GLint pos = glGetAttribLocation (p.getProgramID(), "position");
    if (pos < 0) return;
    glBindBuffer (GL_ARRAY_BUFFER, vbo);
    glEnableVertexAttribArray ((GLuint) pos);
    glVertexAttribPointer ((GLuint) pos, 2, GL_FLOAT, GL_FALSE, 0, nullptr);
    glDrawArrays (GL_TRIANGLE_STRIP, 0, 4);
    glDisableVertexAttribArray ((GLuint) pos);
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void GpuRoom::render (const RoomUniforms& u, int w, int h)
{
    if (prog == nullptr || vbo == 0) return;
    GLint previous = 0;
    glGetIntegerv (GL_FRAMEBUFFER_BINDING, &previous);
    bool glow = bloom != nullptr && ctx != nullptr;
    const float sc = glow ? juce::jlimit (0.3f, 1.0f, u.scale) : 1.0f;     // quality: the scene is drawn smaller and scaled up by the bloom pass
    const int sw = juce::jmax (64, (int) (w * sc)), sh = juce::jmax (64, (int) (h * sc));
    if (glow && (fbW != sw || fbH != sh))
    {
        fb.release();
        glow = fb.initialise (*ctx, sw, sh);
        fbW = glow ? sw : 0; fbH = glow ? sh : 0;
    }
    if (glow) glBindFramebuffer (GL_FRAMEBUFFER, fb.getFrameBufferID());
    glViewport (0, 0, glow ? sw : w, glow ? sh : h);
    glDisable (GL_BLEND); glDisable (GL_CULL_FACE); glDisable (GL_DEPTH_TEST); glDisable (GL_SCISSOR_TEST);
    prog->use();
    prog->setUniform ("uRes", (GLfloat) sw, (GLfloat) sh);
    prog->setUniform ("uTime", u.time); prog->setUniform ("uMemory", u.memory); prog->setUniform ("uAge", u.age);
    prog->setUniform ("uWorld", u.world); prog->setUniform ("uEnergy", u.energy); prog->setUniform ("uHit", u.hit);
    prog->setUniform ("uPedal", u.pedal); prog->setUniform ("uFlame", u.flame); prog->setUniform ("uIntimacy", u.intimacy);
    prog->setUniform ("uWarmth", u.warmth); prog->setUniform ("uSpace", u.space);
    prog->setUniform ("uKeys", u.keys, 88);
    {
        const auto& v = u.look.v;
        prog->setUniform ("uTop", v[kTopR], v[kTopG], v[kTopB]); prog->setUniform ("uBot", v[kBotR], v[kBotG], v[kBotB]); prog->setUniform ("uSunCol", v[kSunR], v[kSunG], v[kSunB]);
        prog->setUniform ("uGround", v[kGrR], v[kGrG], v[kGrB]); prog->setUniform ("uFoliage", v[kFoR], v[kFoG], v[kFoB]);
        prog->setUniform ("uSunI", v[kSunI]); prog->setUniform ("uSunPx", v[kSunPx]); prog->setUniform ("uSunPy", v[kSunPy]); prog->setUniform ("uDirX", v[kDirX]); prog->setUniform ("uDirY", v[kDirY]);
        prog->setUniform ("uAmb", v[kAmb]); prog->setUniform ("uNight", v[kNight]); prog->setUniform ("uCloud", v[kCloud]); prog->setUniform ("uWind", v[kWind]); prog->setUniform ("uRain", v[kRain]);
        prog->setUniform ("uOutL", v[kOutL]); prog->setUniform ("uFoAmt", v[kFoAmt]); prog->setUniform ("uSnow", v[kSnow]); prog->setUniform ("uBlossom", v[kBlossom]); prog->setUniform ("uLeaf", v[kLeaf]);
        prog->setUniform ("uSeason", v[kSeason]); prog->setUniform ("uHours", v[kHours]); prog->setUniform ("uSeed", u.seed); prog->setUniform ("uSection", u.section);
    }
    drawQuad (*prog);
    if (glow)
    {
        glBindFramebuffer (GL_FRAMEBUFFER, (GLuint) previous);
        glViewport (0, 0, w, h);
        bloom->use();
        glActiveTexture (GL_TEXTURE0);
        glBindTexture (GL_TEXTURE_2D, fb.getTextureID());
        bloom->setUniform ("uTex", (GLint) 0);
        bloom->setUniform ("uRes", (GLfloat) w, (GLfloat) h);
        bloom->setUniform ("uAmt", 1.0f + 0.5f * u.memory);
        drawQuad (*bloom);
        glBindTexture (GL_TEXTURE_2D, 0);
    }
}

// ------------------------------------------------------------------ OffscreenRoom
struct OffscreenRoom::Host : public juce::Component
{
    juce::OpenGLContext context;
    GpuRoom room;
    Host()
    {
        setSize (64, 64);
        setOpaque (true);
        addToDesktop (juce::ComponentPeer::windowIsTemporary | juce::ComponentPeer::windowIgnoresKeyPresses);
        setTopLeftPosition (-20000, -20000);
        setVisible (true);
        context.setComponentPaintingEnabled (false);
        context.setContinuousRepainting (false);
        context.attachTo (*this);
    }
    ~Host() override { context.detach(); }
};

OffscreenRoom& OffscreenRoom::get() { static OffscreenRoom o; return o; }

namespace
{
template <typename Fn>
bool runOnGl (juce::OpenGLContext& ctx, Fn&& fn, int timeoutMs)
{
    auto done = std::make_shared<std::atomic<bool>> (false);
    ctx.executeOnGLThread ([fn, done] (juce::OpenGLContext& c) mutable { fn (c); done->store (true); }, false);
    ctx.triggerRepaint();
    const auto end = juce::Time::getMillisecondCounter() + (juce::uint32) timeoutMs;
    while (! done->load() && juce::Time::getMillisecondCounter() < end)
    {
        if (juce::MessageManager::getInstance()->isThisTheMessageThread()) juce::MessageManager::getInstance()->runDispatchLoopUntil (2);
        else juce::Thread::sleep (1);
        ctx.triggerRepaint();
    }
    return done->load();
}
}

bool OffscreenRoom::available()
{
    if (tried) return ok;
    tried = true;
    if (gpuStatus().load() < 0) return ok = false;
    host = std::make_unique<Host>();
    for (int i = 0; i < 200 && ! host->context.isAttached(); ++i) juce::MessageManager::getInstance()->runDispatchLoopUntil (10);
    if (! host->context.isAttached()) { host.reset(); return ok = false; }
    auto good = std::make_shared<bool> (false);
    auto* h = host.get();
    const bool ran = runOnGl (host->context, [h, good] (juce::OpenGLContext& c) { *good = h->room.init (c); }, 8000);
    ok = ran && *good;
    if (ran && ! *good) { gpuStatus() = -1; std::puts (h->room.error.toRawUTF8()); }
    if (! ok) host.reset();
    return ok;
}

juce::Image OffscreenRoom::render (const RoomUniforms& u, int w, int h)
{
    if (! available()) return {};
    juce::Image img (juce::Image::ARGB, w, h, false, juce::SoftwareImageType());
    bool done = false;
    auto* hst = host.get();
    const bool ran = runOnGl (host->context, [&, hst] (juce::OpenGLContext& c)
    {
        juce::OpenGLFrameBuffer fb;
        if (! fb.initialise (c, w, h)) return;
        fb.makeCurrentRenderingTarget();
        hst->room.render (u, w, h);
        const juce::Image::BitmapData bd (img, juce::Image::BitmapData::writeOnly);
        std::vector<juce::PixelARGB> px ((size_t) (w * h));
        fb.readPixels (px.data(), { 0, 0, w, h }, juce::OpenGLFrameBuffer::RowOrder::fromTopDown);
        for (int y = 0; y < h; ++y) std::memcpy (bd.getLinePointer (y), px.data() + (size_t) y * (size_t) w, (size_t) w * 4);
        fb.releaseAsRenderingTarget();
        done = true;
    }, 5000);
    return ran && done ? img : juce::Image();
}

void OffscreenRoom::shutdown() { host.reset(); tried = false; ok = false; }
}
