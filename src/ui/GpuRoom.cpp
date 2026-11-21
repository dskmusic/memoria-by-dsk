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

float hash12 (vec2 p) { vec3 p3 = fract (vec3 (p.xyx) * 0.1031); p3 += dot (p3, p3.yzx + 33.33); return fract ((p3.x + p3.y) * p3.z); }
vec2 hash22 (vec2 p) { vec3 p3 = fract (vec3 (p.xyx) * vec3 (0.1031, 0.1030, 0.0973)); p3 += dot (p3, p3.yzx + 33.33); return fract ((p3.xx + p3.yz) * p3.zy); }
float noise (vec2 p)
{
    vec2 i = floor (p), f = fract (p);
    f = f * f * (3.0 - 2.0 * f);
    return mix (mix (hash12 (i), hash12 (i + vec2 (1.0, 0.0)), f.x), mix (hash12 (i + vec2 (0.0, 1.0)), hash12 (i + vec2 (1.0, 1.0)), f.x), f.y);
}
float fbm (vec2 p)
{
    float v = 0.0, a = 0.5;
    mat2 m = mat2 (1.6, 1.2, -1.2, 1.6);
    for (int i = 0; i < 5; i++) { v += a * noise (p); p = m * p; a *= 0.5; }
    return v;
}
float sdBox (vec2 p, vec2 b) { vec2 d = abs (p) - b; return length (max (d, 0.0)) + min (max (d.x, d.y), 0.0); }
float sdRound (vec2 p, vec2 b, float r) { return sdBox (p, b - vec2 (r)) - r; }
float sdSeg (vec2 p, vec2 a, vec2 b) { vec2 pa = p - a, ba = b - a; float h = clamp (dot (pa, ba) / dot (ba, ba), 0.0, 1.0); return length (pa - ba * h); }
float aa (float d) { return clamp (0.5 - d * 380.0, 0.0, 1.0); }

// window and light shaft (light travels towards +x, -y)
const float WX0 = -0.64, WX1 = -0.26, WY0 = -0.15, WY1 = 0.39;
const vec2 LD = vec2 (0.70, -0.55);
float beamMask (vec2 q)
{
    float lo = max (0.0, max ((q.x - WX1) / LD.x, (WY0 - q.y) / (-LD.y)));
    float hi = min ((q.x - WX0) / LD.x, (WY1 - q.y) / (-LD.y));
    if (hi <= lo) return 0.0;
    float s = 0.5 * (lo + hi);
    vec2 wq = q - s * LD;
    float wu = (wq.x - WX0) / (WX1 - WX0), wv = (wq.y - WY0) / (WY1 - WY0);
    float mull = max (max (smoothstep (0.045, 0.02, abs (wu - 0.5)), smoothstep (0.03, 0.012, abs (wv - 0.34))), smoothstep (0.03, 0.012, abs (wv - 0.67)));
    return smoothstep (0.0, 0.09, hi - lo) * exp (-lo * 1.15) * (1.0 - 0.75 * mull);
}

vec3 skyColour (vec2 w)
{
    int wd = int (uWorld + 0.5);
    vec3 top = vec3 (0.62, 0.46, 0.34), bot = vec3 (1.0, 0.80, 0.50);
    float sunI = 1.0; vec2 sunP = vec2 (0.68, 0.30);
    if (wd == 1) { top = vec3 (0.36, 0.42, 0.50); bot = vec3 (0.58, 0.62, 0.68); sunI = 0.0; }
    else if (wd == 2) { top = vec3 (0.12, 0.16, 0.32); bot = vec3 (0.95, 0.55, 0.28); sunI = 0.4; sunP = vec2 (0.3, 0.2); }
    else if (wd == 3) { top = vec3 (0.01, 0.02, 0.06); bot = vec3 (0.05, 0.07, 0.15); sunI = 0.0; }
    else if (wd == 4) { top = vec3 (0.45, 0.38, 0.62); bot = vec3 (1.0, 0.68, 0.46); sunI = 0.9; sunP = vec2 (0.55, 0.22); }
    else if (wd == 5) { top = vec3 (0.18, 0.12, 0.30); bot = vec3 (0.85, 0.42, 0.38); sunI = 0.3; sunP = vec2 (0.25, 0.2); }
    vec3 c = mix (bot, top, pow (clamp (w.y, 0.0, 1.0), 0.6));
    float d = length ((w - sunP) * vec2 (1.0, 1.2));
    c += vec3 (1.0, 0.8, 0.5) * sunI * (exp (-d * 6.0) * 0.7 + smoothstep (0.045, 0.03, d) * 1.2);
    float cl = fbm (vec2 (w.x * 3.0 + uTime * 0.01, w.y * 6.0));
    c = mix (c, c * 0.7 + vec3 (0.15), smoothstep (0.5, 0.8, cl) * (wd == 1 ? 0.8 : 0.35));
    if (wd == 3)
    {
        vec2 g = floor (w * 44.0);
        float st = step (0.985, hash12 (g));
        c += st * vec3 (0.7, 0.75, 0.9) * (0.5 + 0.5 * sin (uTime * 2.0 + hash12 (g) * 30.0));
    }
    float hill = 0.22 + 0.05 * sin (w.x * 8.0) + 0.03 * sin (w.x * 21.0 + 1.0);
    vec3 dark = c * 0.09 + vec3 (0.0, 0.004, 0.01);
    vec2 t = w - vec2 (0.25, 0.34);
    float crown = length (t * vec2 (2.2, 1.0)) - 0.14;
    if (w.y < hill || crown < 0.0 || (abs (t.x) < 0.012 && w.y < 0.34 && w.y > 0.12)) c = dark;
    if (wd == 2 && w.y < hill + 0.04 && w.y > hill - 0.12)
    {
        float lamp = step (0.72, hash12 (vec2 (floor (w.x * 18.0), 1.0)));
        c += vec3 (1.0, 0.7, 0.3) * lamp * smoothstep (0.12, 0.0, length (vec2 (fract (w.x * 18.0) - 0.5, (w.y - hill) * 5.0))) * 0.9;
    }
    return c * 1.25;
}

// rain on the glass: beads and sliding drops; xy = how they bend the view, z = brightness
vec3 rainDrops (vec2 uv, float t)
{
    vec3 r = vec3 (0.0);
    for (int k = 0; k < 3; k++)
    {
        float fk = float (k), s = 7.0 + 6.0 * fk;
        vec2 g = uv * vec2 (s * 0.75, s) + vec2 (fk * 7.3, fk * 2.1);
        vec2 id = floor (g), f = fract (g) - 0.5;
        float rn = hash12 (id + fk * 13.7), rn2 = hash12 (id + 5.1 + fk);
        float spd = 0.08 + 0.35 * rn2;
        vec2 pos = vec2 ((rn - 0.5) * 0.5, 0.38 - 0.76 * fract (t * spd + rn * 9.0));
        float rad = 0.05 + 0.06 * rn2;
        float d = length (f - pos);
        float drop = smoothstep (rad, rad * 0.35, d);
        float trail = smoothstep (0.03, 0.0, abs (f.x - pos.x)) * step (pos.y, f.y) * smoothstep (pos.y + 0.4, pos.y, f.y) * 0.35;
        float on = step (0.35, rn2);
        r.xy += (f - pos) * drop * 0.9 * on;
        r.z += (drop * 0.9 + trail * 1.4) * on;
    }
    return r;
}

vec3 worldBeam()
{
    int wd = int (uWorld + 0.5);
    if (wd == 1) return vec3 (0.55, 0.65, 0.85) * 0.30;
    if (wd == 2) return vec3 (1.0, 0.60, 0.35) * 0.55;
    if (wd == 3) return vec3 (0.45, 0.55, 0.95) * 0.20;
    if (wd == 4) return vec3 (1.0, 0.70, 0.55) * 0.85;
    if (wd == 5) return vec3 (0.85, 0.50, 0.70) * 0.45;
    return vec3 (1.0, 0.78, 0.50);
}
)GLSL" R"GLSL(
vec3 scene (vec2 uv)
{
    float asp = uRes.x / uRes.y;
    vec2 p = (uv - 0.5) * vec2 (asp, 1.0);
    int wd = int (uWorld + 0.5);
    vec3 bc = worldBeam();
    float night = wd == 3 ? 1.0 : 0.0;
    float amb = wd == 1 ? 0.55 : wd == 3 ? 0.30 : wd == 2 ? 0.75 : 1.0;
    float flick = uFlame * (0.94 + 0.06 * sin (uTime * 11.0)) + 0.18 * uEnergy + 0.12 * uHit;
    vec2 cp = vec2 (0.66, 0.247);                                   // candle base
    vec2 fp = cp + vec2 (0.0012 * sin (uTime * 6.1), 0.097);       // flame
    float cdist = length ((p - fp) * vec2 (1.0, 1.0));
    float candleL = exp (-cdist * cdist * 22.0) * flick * (wd == 3 ? 1.8 : 1.0);
    vec3 candleC = vec3 (1.0, 0.58, 0.24);

    // ---------- materials: albedo and gloss
    vec3 alb = vec3 (0.0); float gloss = 0.0; float emit = 0.0; vec3 emitC = vec3 (0.0);
    // wall
    float stripe = 1.0 + 0.06 * sin (p.x * 150.0) + 0.05 * fbm (p * 9.0);
    alb = vec3 (0.20, 0.13, 0.10) * stripe;
    float wain = smoothstep (0.0, 0.004, -0.02 - p.y);
    if (p.y < -0.02) alb = mix (alb, vec3 (0.12, 0.075, 0.05) * (0.9 + 0.2 * fbm (p * vec2 (3.0, 14.0))), 1.0);
    if (abs (p.y + 0.02) < 0.006) alb = vec3 (0.26, 0.17, 0.11);
    // picture frame
    vec2 pf = p - vec2 (0.28, 0.30);
    float dfr = sdBox (pf, vec2 (0.11, 0.075));
    if (dfr < 0.0) alb = mix (vec3 (0.05, 0.035, 0.03), mix (vec3 (0.35, 0.18, 0.12), vec3 (0.20, 0.30, 0.38), fbm (pf * 9.0 + 3.0)), step (0.012, -dfr));
    // floor
    if (p.y < -0.27)
    {
        float t = (-0.27 - p.y) / 0.23;
        float seam = smoothstep (0.012, 0.0, abs (fract ((p.x + t * 0.15) * 7.0) - 0.5) - 0.47);
        alb = vec3 (0.14, 0.085, 0.052) * (0.8 + 0.45 * fbm (vec2 (p.x * 3.0, p.y * 40.0))) * (0.55 + 0.6 * t) * (1.0 - 0.5 * seam);
        gloss = 0.35;
        // rug
        vec2 rg = p - vec2 (0.25, -0.38);
        if (sdRound (rg, vec2 (0.50, 0.075), 0.02) < 0.0)
        {
            float pat = 0.5 + 0.5 * sin (rg.x * 90.0) * sin (rg.y * 230.0);
            alb = mix (vec3 (0.28, 0.07, 0.06), vec3 (0.50, 0.28, 0.14), pat) * 0.55;
            gloss = 0.0;
        }
    }
    else if (p.y < -0.23) alb = vec3 (0.20, 0.13, 0.085);          // baseboard
    // curtains
    bool curL = p.x > -0.72 && p.x < -0.64 && p.y > -0.20 && p.y < 0.47;
    bool curR = p.x > -0.26 && p.x < -0.18 && p.y > -0.20 && p.y < 0.47;
    if (curL || curR)
    {
        float fold = 0.55 + 0.45 * sin (p.x * 300.0 + (curR ? 1.0 : 0.0) + 0.4 * sin (uTime * 0.4 + p.y * 3.0));
        alb = vec3 (0.34, 0.09, 0.08) * fold;
    }
    // bench (in front of the piano)
    if (sdBox (p - vec2 (0.30, -0.31), vec2 (0.14, 0.022)) < 0.0) { alb = vec3 (0.10, 0.06, 0.04); gloss = 0.3; }
    if (sdBox (p - vec2 (0.19, -0.38), vec2 (0.008, 0.05)) < 0.0 || sdBox (p - vec2 (0.41, -0.38), vec2 (0.008, 0.05)) < 0.0) alb = vec3 (0.07, 0.045, 0.03);
    // upright piano seen from the front
    bool onPiano = false;
    vec2 pb = p - vec2 (0.33, 0.0);
    float dBody = sdRound (pb, vec2 (0.40, 0.235), 0.02);
    float dLedge = sdBox (p - vec2 (0.33, 0.245), vec2 (0.425, 0.012));
    if (dBody < 0.0 || dLedge < 0.0)
    {
        alb = vec3 (0.020, 0.015, 0.017) * (1.0 + 0.4 * fbm (p * vec2 (5.0, 40.0))); gloss = 0.9; onPiano = true;
        float panel1 = sdRound (p - vec2 (0.33, 0.145), vec2 (0.32, 0.065), 0.01);
        float panel2 = sdRound (p - vec2 (0.33, -0.15), vec2 (0.31, 0.048), 0.01);
        if (abs (panel1) < 0.004 || abs (panel2) < 0.004) alb = vec3 (0.06, 0.045, 0.04);
        else if (panel1 < 0.0 || panel2 < 0.0) alb *= 0.65;
        alb += vec3 (0.11, 0.085, 0.06) * smoothstep (0.006, 0.0, abs (dBody + 0.004));
        alb += vec3(0.12, 0.09, 0.06) * smoothstep (0.004, 0.0, abs (dLedge));
    }
    // music desk with a sheet
    vec2 md = p - vec2 (0.33, 0.045);
    if (sdBox (md, vec2 (0.15, 0.065)) < 0.0)
    {
        bool sheet = abs (md.x) < 0.125 && abs (md.y) < 0.052;
        float lines = step (0.45, fract (md.y * 160.0));
        alb = sheet ? vec3 (0.68, 0.63, 0.52) * (1.0 - 0.22 * lines * step (0.0, md.y + 0.03)) : vec3 (0.035, 0.028, 0.028);
        gloss = 0.0; onPiano = true;
    }
    // keys
    vec2 kb = p - vec2 (0.33, -0.047);
    if (sdBox (kb, vec2 (0.30, 0.018)) < 0.0)
    {
        float kx = (p.x - 0.03) * 86.7;
        float ki = floor (kx), kf = fract (kx);
        float mk = mod (ki, 7.0);
        bool blk = (mk < 1.5 || (mk > 2.5 && mk < 5.5)) && kf > 0.62 && kb.y > -0.003;
        alb = blk ? vec3 (0.015) : (kf < 0.06 ? vec3 (0.25) : vec3 (0.84, 0.80, 0.72));
        gloss = 0.2; onPiano = true;
    }
    // foot rail and pedals
    if (sdBox (p - vec2 (0.33, -0.245), vec2 (0.41, 0.012)) < 0.0) { alb = vec3 (0.022); gloss = 0.7; onPiano = true; }
    if (sdBox (p - vec2 (0.27, -0.236), vec2 (0.016, 0.008)) < 0.0 || sdBox (p - vec2 (0.39, -0.236), vec2 (0.016, 0.008)) < 0.0) alb = vec3 (0.55, 0.42, 0.20);
    // candle
    if (abs (p.x - cp.x) < 0.0085 && p.y > cp.y + 0.004 && p.y < cp.y + 0.082) { alb = vec3 (0.92, 0.84, 0.66); onPiano = false; }

    // ---------- lighting
    float bm = beamMask (p);
    float shimmer = 0.78 + 0.45 * fbm (p * 5.5 + vec2 (uTime * 0.02, -uTime * 0.014));
    float beamI = bm * (0.9 + 0.35 * uEnergy);
    vec3 lit = alb * amb * 0.42;
    lit += alb * bc * beamI * 1.25;                                          // sunlight on surfaces
    lit += alb * candleC * candleL * 2.2;                                   // candle light
    lit += alb * vec3 (0.8, 0.6, 0.5) * night * 0.04;
    // glossy reflections (the window and the candle in the piano and the floor)
    if (gloss > 0.0)
    {
        float streak = smoothstep (0.25, 0.0, abs (fract (p.x * 2.2 + p.y * 0.8) - 0.5) - 0.18);
        float refl = gloss * (0.03 + 0.28 * bm) * (0.5 + 0.5 * streak) * (0.7 + 0.5 * fbm (p * 8.0));
        lit += bc * refl * (onPiano ? 0.8 : 0.35);
        lit += candleC * gloss * candleL * 0.35 * (onPiano ? 1.0 : 0.3);
    }
    lit += emitC * emit * bc * (0.2 + bm * 1.5);
    // sustain pedal: the piano glows faintly warm while it rings
    if (onPiano) lit += vec3 (0.5, 0.3, 0.14) * 0.03 * uPedal * (0.5 + uEnergy);

    // ---------- the window: sky, rain, frame
    if (p.x > WX0 && p.x < WX1 && p.y > WY0 && p.y < WY1)
    {
        vec2 wu = vec2 ((p.x - WX0) / (WX1 - WX0), (p.y - WY0) / (WY1 - WY0));
        bool bar = wu.x < 0.03 || wu.x > 0.97 || wu.y < 0.025 || wu.y > 0.975 || abs (wu.x - 0.5) < 0.018 || abs (wu.y - 0.34) < 0.012 || abs (wu.y - 0.67) < 0.012;
        if (bar) lit = vec3 (0.10, 0.065, 0.045) * (0.5 + bm * 0.5 + 0.4 * amb);
        else
        {
            vec3 rd = wd == 1 ? rainDrops (wu * vec2 (1.0, 1.4), uTime) : vec3 (0.0);
            vec3 sc = skyColour (wu + rd.xy * 0.06);
            if (wd == 1) sc = mix (sc, vec3 (dot (sc, vec3 (0.33))), 0.2) * (0.80 + rd.z * 0.45) + rd.z * 0.22;
            lit = sc;
        }
    }
    // ---------- light in the air: a shaft with floating dust
    float air = 0.0;
    for (int i = 0; i < 14; i++)
    {
        float s = (float (i) + 0.5) / 14.0 * 0.9;
        vec2 q = p - LD * s * 0.55;
        float dust = fbm (q * 7.0 + vec2 (uTime * 0.03, -uTime * 0.02));
        air += beamMask (q) * (0.55 + 0.9 * dust);
    }
    lit += bc * air * 0.0105 * (0.9 + 0.5 * uEnergy);
    // motes
    for (int l = 0; l < 3; l++)
    {
        float fl = float (l);
        vec2 g = p * (24.0 + 13.0 * fl) + vec2 (uTime * (0.25 + 0.1 * fl) * 0.3, -uTime * (0.35 + 0.2 * fl) * 0.25 + uHit * 0.4 * fl);
        vec2 id = floor (g), f = fract (g);
        vec2 o = 0.2 + 0.6 * hash22 (id + fl * 9.1);
        o += 0.12 * vec2 (sin (uTime * (0.5 + hash12 (id)) + hash12 (id + 3.0) * 6.0), cos (uTime * (0.4 + hash12 (id + 1.0)) + hash12 (id + 5.0) * 6.0));
        float d = length (f - o);
        float tw = 0.55 + 0.45 * sin (uTime * (1.0 + 2.0 * hash12 (id + 7.0)) + hash12 (id) * 20.0);
        float inB = beamMask (p);
        float notWin = (p.x > WX0 && p.x < WX1 && p.y > WY0 && p.y < WY1) ? 0.0 : 1.0;
        lit += bc * smoothstep (0.085 + 0.02 * fl, 0.0, d) * tw * inB * 1.3 * step (0.80, hash12 (id + 2.0)) * notWin;
    }
    // candle flame
    {
        vec2 fq = (p - fp) * vec2 (1.0, 1.0);
        fq.x += 0.0016 * sin (uTime * 9.0 + fq.y * 80.0) * smoothstep (-0.01, 0.03, fq.y);
        float shape = length (fq * vec2 (1.0, 0.42)) - 0.014 * (1.0 - 0.3 * smoothstep (0.0, 0.03, fq.y));
        float core = smoothstep (0.012, 0.0, length (fq * vec2 (1.0, 0.5) + vec2 (0.0, 0.004)));
        lit += mix (vec3 (1.0, 0.55, 0.15), vec3 (1.0, 0.95, 0.8), core) * smoothstep (0.004, -0.004, shape) * 2.4 * flick;
        lit += candleC * exp (-length (fq) * 38.0) * 0.55 * flick;
    }
    // warmth / space move the colour grade a little
    lit *= mix (vec3 (1.0), vec3 (1.08, 1.0, 0.88), uWarmth);
    return lit;
}
)GLSL" R"GLSL(
vec3 aces (vec3 x) { return clamp ((x * (2.51 * x + 0.03)) / (x * (2.43 * x + 0.59) + 0.14), 0.0, 1.0); }

void main()
{
    vec2 uv0 = gl_FragCoord.xy / uRes;
    float aging = clamp (uMemory * (0.55 + 0.45 * uAge), 0.0, 1.0);
    // slow cinematic camera, a film gate that weaves when the tape is old
    float z = 1.04 + 0.025 * sin (uTime * 0.07) + 0.01 * sin (uTime * 0.19 + 2.0);
    vec2 pan = vec2 (0.012 * sin (uTime * 0.05), 0.006 * sin (uTime * 0.07 + 1.0));
    vec2 uv = (uv0 - 0.5) / z + 0.5 + pan;
    if (aging > 0.3)
    {
        uv.y += (hash12 (vec2 (floor (uTime * 14.0), 2.0)) - 0.5) * 0.004 * aging;
        uv.x += 0.0012 * aging * sin (uv0.y * 60.0 + uTime * 3.0) * (0.5 + 0.5 * sin (uTime * 0.6));
    }
    vec3 c = scene (uv);
    // film: highlights bloom slightly (the second pass adds the rest), tone-map
    c = aces (c * 0.95);
    // memory: sepia, faded blacks, vignette, light leak, scratches, dust, grain
    float lum = dot (c, vec3 (0.3, 0.59, 0.11));
    c = mix (c, vec3 (lum * 1.14, lum * 0.93, lum * 0.66), 0.82 * aging);
    c += vec3 (0.055, 0.048, 0.036) * aging;
    c = mix (c, c * c * (3.0 - 2.0 * c), 0.0);
    vec2 v = (uv0 - 0.5) * vec2 (1.0, 1.1);
    c *= 1.0 - (0.35 + 0.45 * aging) * pow (clamp (length (v) * 1.3, 0.0, 1.0), 2.2);
    c += vec3 (1.0, 0.45, 0.18) * smoothstep (0.82, 1.0, uv0.x) * (0.12 + 0.08 * sin (uTime * 0.7)) * smoothstep (0.4, 0.9, aging);
    float sc = 0.0;
    for (int i = 0; i < 3; i++)
    {
        float fi = float (i);
        float sx = hash12 (vec2 (floor (uTime * 9.0), fi * 3.1));
        sc += smoothstep (0.0007, 0.0, abs (uv0.x - sx)) * step (0.35, hash12 (vec2 (fi, floor (uTime * 9.0) + 2.0)));
    }
    c += vec3 (0.20, 0.18, 0.14) * sc * smoothstep (0.25, 0.7, aging);
    vec2 dg = floor (uv0 * uRes / 9.0 + floor (uTime * 12.0) * 31.0);
    float spk = step (0.9985, hash12 (dg)) * smoothstep (0.3, 0.8, aging);
    c = mix (c, vec3 (0.02), spk * 0.6);
    c += (hash12 (uv0 * uRes + fract (uTime * 7.3) * 113.0) - 0.5) * (0.012 + 0.07 * aging);
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
bool GpuRoom::init (juce::OpenGLContext& c)
{
    ctx = &c;
    error.clear();
    {
        auto p = std::make_unique<juce::OpenGLShaderProgram> (c);
        const juce::String fs = juce::String ("uniform float uPedalDummy;\n") + roomShader;
        if (! p->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader))
            || ! p->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (roomShader))
            || ! p->link())
        {
            error << "room: " << p->getLastError() << "\n";
            DBG (error);
            (void) fs;
            auto log = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory).getChildFile ("DSK Music").getChildFile ("Memoria").getChildFile ("gpu-log.txt");
            log.getParentDirectory().createDirectory();
            log.replaceWithText ("Memoria GPU scene\nOpenGL: " + juce::String ((const char*) glGetString (GL_VERSION)) + "  " + juce::String ((const char*) glGetString (GL_RENDERER)) + "\n\n" + error);
            return false;
        }
        prog = std::move (p);
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
    if (glow && (fbW != w || fbH != h))
    {
        fb.release();
        glow = fb.initialise (*ctx, w, h);
        fbW = glow ? w : 0; fbH = glow ? h : 0;
    }
    if (glow) glBindFramebuffer (GL_FRAMEBUFFER, fb.getFrameBufferID());
    glViewport (0, 0, w, h);
    glDisable (GL_BLEND); glDisable (GL_CULL_FACE); glDisable (GL_DEPTH_TEST); glDisable (GL_SCISSOR_TEST);
    prog->use();
    prog->setUniform ("uRes", (GLfloat) w, (GLfloat) h);
    prog->setUniform ("uTime", u.time); prog->setUniform ("uMemory", u.memory); prog->setUniform ("uAge", u.age);
    prog->setUniform ("uWorld", u.world); prog->setUniform ("uEnergy", u.energy); prog->setUniform ("uHit", u.hit);
    prog->setUniform ("uPedal", u.pedal); prog->setUniform ("uFlame", u.flame); prog->setUniform ("uIntimacy", u.intimacy);
    prog->setUniform ("uWarmth", u.warmth); prog->setUniform ("uSpace", u.space);
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
