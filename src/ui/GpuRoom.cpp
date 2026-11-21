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
const vec3 SUNL = vec3 (-0.33, 0.40, -0.85);       // direction towards the light (through the window in the back wall)

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
    if (uWorld > 1.5 && uWorld < 2.5) fold += 0.07 * (0.5 + 0.5 * sin (uTime * 0.31)) * (1.9 - p.y * 0.5) * sin (uTime * 1.3 + p.y * 1.8 + p.x * 3.0);
    r = opU (r, vec2 (sdBox (p - vec3 (WX0 - 0.22, 1.62, -2.86), vec3 (0.2 + fold, 1.55, 0.12)), 8.0));
    r = opU (r, vec2 (sdBox (p - vec3 (WX1 + 0.22, 1.62, -2.86), vec3 (0.2 + fold, 1.55, 0.12)), 8.0));
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

vec3 skyColour (vec2 w)
{
    int wd = int (uWorld + 0.5);
    vec3 top = vec3 (0.62, 0.46, 0.34), bot = vec3 (1.0, 0.80, 0.50);
    float sunI = 1.0; vec2 sunP = vec2 (0.68, 0.30);
    if (wd == 1) { top = vec3 (0.36, 0.42, 0.50); bot = vec3 (0.58, 0.62, 0.68); sunI = 0.0; }
    else if (wd == 2) { top = vec3 (0.38, 0.52, 0.72); bot = vec3 (0.88, 0.86, 0.80); sunI = 0.55; sunP = vec2 (0.35, 0.3); }
    else if (wd == 3) { top = vec3 (0.01, 0.02, 0.06); bot = vec3 (0.05, 0.07, 0.15); sunI = 0.0; }
    else if (wd == 4) { top = vec3 (0.45, 0.38, 0.62); bot = vec3 (1.0, 0.68, 0.46); sunI = 0.9; sunP = vec2 (0.55, 0.22); }
    else if (wd == 5) { top = vec3 (0.18, 0.12, 0.30); bot = vec3 (0.85, 0.42, 0.38); sunI = 0.3; sunP = vec2 (0.25, 0.2); }
    vec3 c = mix (bot, top, pow (clamp (w.y, 0.0, 1.0), 0.6));
    float d = length ((w - sunP) * vec2 (1.0, 1.2));
    c += vec3 (1.0, 0.8, 0.5) * sunI * (exp (-d * 6.0) * 0.7 + ss (0.045, 0.03, d) * 1.2);
    float cl = fbm (vec2 (w.x * 3.0 + uTime * (wd == 2 ? 0.09 : 0.01), w.y * 6.0));
    c = mix (c, c * 0.7 + vec3 (0.15), ss (0.5, 0.8, cl) * (wd == 1 ? 0.8 : 0.35));
    if (wd == 3)
    {
        vec2 g = floor (w * 44.0);
        c += step (0.985, hash12 (g)) * vec3 (0.7, 0.75, 0.9) * (0.5 + 0.5 * sin (uTime * 2.0 + hash12 (g) * 30.0));
    }
    float hill = 0.22 + 0.05 * sin (w.x * 8.0) + 0.03 * sin (w.x * 21.0 + 1.0);
    vec3 dark = c * 0.09 + vec3 (0.0, 0.004, 0.01);
    vec2 t = w - vec2 (0.25, 0.34);
    if (wd == 2) t.x += 0.012 * sin (uTime * 1.7 + w.y * 6.0) * (0.5 + 0.5 * sin (uTime * 0.4));
    float crown = length (t * vec2 (2.2, 1.0)) - 0.14;
    if (w.y < hill || crown < 0.0 || (abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12)) c = dark;
    return c * 1.25;
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
    vec3 rd = int (uWorld + 0.5) == 1 ? rainDrops (wu * vec2 (1.3, 1.0), uTime) : vec3 (0.0);
    vec3 sc = skyColour (wu + rd.xy * 0.06);
    if (int (uWorld + 0.5) == 1) sc = sc * (0.80 + rd.z * 0.45) + rd.z * 0.22;
    return sc;
}

vec3 sunColour()
{
    int wd = int (uWorld + 0.5);
    if (wd == 1) return vec3 (0.55, 0.65, 0.85) * 0.35;
    if (wd == 2) return vec3 (0.95, 0.90, 0.80) * 0.85;
    if (wd == 3) return vec3 (0.45, 0.55, 0.95) * 0.22;
    if (wd == 4) return vec3 (1.0, 0.70, 0.55) * 1.0;
    if (wd == 5) return vec3 (0.85, 0.50, 0.70) * 0.55;
    return vec3 (1.0, 0.78, 0.50) * 1.25;
}

// key index (0..87) under a point of the keyboard, and which key it is
float keyDown (float midiIdx)
{
    float v = 0.0;
#ifdef LITE
    return v;
#endif
)GLSL" R"GLSL(
    for (int i = 0; i < 88; i++) if (float (i) == midiIdx) v = uKeys[i];
    return v;
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
        vec3 art = mix (vec3 (0.18, 0.26, 0.32), vec3 (0.60, 0.40, 0.22), fbm (u * 5.0 + 2.0));
        alb = mix (vec3 (0.28, 0.17, 0.08), art, inside); rough = 0.5;
    }
    else { alb = vec3 (0.75, 0.55, 0.22); rough = 0.25; f0 = 0.6; }                // brass
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
    float flick = uFlame * (0.94 + 0.06 * sin (uTime * 11.0)) + 0.16 * uEnergy + 0.10 * uHit;
    // the window: sun through the glass with soft shadows, plus sky fill from the opening
    float wl = windowLight (p + n * 0.02);
    float sunD = max (dot (n, SUNL), 0.0);
    float sh = wl > 0.01 && sunD > 0.0 ? softShadow (p + n * 0.02, SUNL, 7.0) : 0.0;
    vec3 col = vec3 (0.0);
    col += alb * sc * sunD * sh * wl * 2.2;
    vec3 wcen = vec3 (-1.6, 1.75, -2.9);
    vec3 toW = wcen - p; float dW = length (toW); toW /= dW;
    float fill = max (dot (n, toW), 0.0) * 1.8 / (1.0 + 0.35 * dW * dW);
    vec3 skyFill = mix (vec3 (0.9, 0.8, 0.7), sc, 0.4) * (wd == 3 ? 0.25 : wd == 1 ? 0.6 : 1.0);
    col += alb * skyFill * fill * ao * 1.1;
    // bounce / ambient
    col += alb * mix (vec3 (0.07, 0.045, 0.03), vec3 (0.15, 0.12, 0.10), n.y * 0.5 + 0.5) * ao * (wd == 3 ? 0.5 : 1.0);
    // the candle
    vec3 pl = FLAMEP + vec3 (0.002 * sin (uTime * 6.0), 0.0, 0.0) - p;
    float dc = length (pl); pl /= dc;
    float cs = dot (n, pl) > 0.0 ? candleShadow (p + n * 0.02, pl, dc) : 0.0;
    vec3 candle = vec3 (1.0, 0.56, 0.22) * flick * (wd == 3 ? 1.6 : 1.0);
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
        float flick = uFlame * (0.94 + 0.06 * sin (uTime * 11.0)) + 0.16 * uEnergy + 0.10 * uHit;
        float shape = dF / (0.011 * (1.0 + 0.2 * sin (uTime * 9.0)));
        col += mix (vec3 (1.0, 0.55, 0.15), vec3 (1.0, 0.95, 0.8), exp (-shape * shape * 0.8)) * exp (-shape * shape * 0.25) * 2.2 * flick;
        col += vec3 (1.0, 0.55, 0.22) * exp (-dF * 22.0 / (0.6 + tF * 0.2)) * 0.30 * flick;
    }
    return col;
}

vec3 aces (vec3 x) { return clamp ((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }

void main()
{
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
)GLSL" R"GLSL(
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
