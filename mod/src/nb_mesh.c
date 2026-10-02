/*
 * Procedural part meshes. Every part model is generated at boot from simple
 * primitives into F3DEX display lists that live in Expansion Pak RAM.
 *
 * Mesh space: centred on the part's footprint, 1 cell = CELL units, +Z is the
 * part's forward, +Y up. Faces are auto-oriented outward from a "centre" hint
 * so back-face culling works without hand-ordering every polygon.
 */
#include "nb.h"

#define VTX_POOL 16000
#define GFX_POOL 20000

static Vtx sVtx[VTX_POOL] __attribute__((aligned(16)));
static Gfx sGfx[GFX_POOL] __attribute__((aligned(8)));
static s32 sVtxUsed, sGfxUsed;

static Gfx *gp;
static Vtx *bv;
static s32 bn, btn;
static u8 bt[64][3];
static u8 sCol[3] = {255, 255, 255};
static f32 sCenter[3];
static f32 sLight[3];

NBMesh nbMeshes[64];
Gfx *nbGridDL, *nbCursorDL, *nbPadDL, *nbShadowDL;

#define H (CELL * 0.5f)

/* ------------------------------------------------------------------ */
/* low level batching                                                  */

static void mb_flush(void) {
    s32 i;
    if (bn == 0) return;
    gSPVertex(gp++, bv, bn, 0);
    for (i = 0; i + 1 < btn; i += 2)
        gSP2Triangles(gp++, bt[i][0], bt[i][1], bt[i][2], 0, bt[i + 1][0], bt[i + 1][1], bt[i + 1][2], 0);
    if (i < btn) gSP1Triangle(gp++, bt[i][0], bt[i][1], bt[i][2], 0);
    sVtxUsed += bn;
    bv = &sVtx[sVtxUsed];
    bn = 0;
    btn = 0;
}

static void mb_reserve(s32 nv, s32 nt) {
    if (bn + nv > 32 || btn + nt > 64) mb_flush();
}

static s32 mb_vtx(const f32 p[3], f32 u, f32 v, f32 bright) {
    Vtx *vt = &bv[bn];
    vt->v.ob[0] = (s16)p[0];
    vt->v.ob[1] = (s16)p[1];
    vt->v.ob[2] = (s16)p[2];
    vt->v.flag = 0;
    vt->v.tc[0] = (s16)(u * 1024.0f);
    vt->v.tc[1] = (s16)(v * 1024.0f);
    vt->v.cn[0] = (u8)nb_clampf(sCol[0] * bright, 0, 255);
    vt->v.cn[1] = (u8)nb_clampf(sCol[1] * bright, 0, 255);
    vt->v.cn[2] = (u8)nb_clampf(sCol[2] * bright, 0, 255);
    vt->v.cn[3] = 255;
    return bn++;
}

static void mb_tri(s32 a, s32 b, s32 c) {
    bt[btn][0] = a; bt[btn][1] = b; bt[btn][2] = c;
    btn++;
}

static Gfx *mb_begin(void) {
    gp = &sGfx[sGfxUsed];
    bv = &sVtx[sVtxUsed];
    bn = btn = 0;
    v3_set(sCenter, 0, 0, 0);
    sCol[0] = sCol[1] = sCol[2] = 255;
    return gp;
}

static Gfx *mb_end(Gfx *start) {
    mb_flush();
    gSPEndDisplayList(gp++);
    sGfxUsed = gp - sGfx;
    return start;
}

static void color(u8 r, u8 g, u8 b) { sCol[0] = r; sCol[1] = g; sCol[2] = b; }
static void center(f32 x, f32 y, f32 z) { v3_set(sCenter, x, y, z); }

/* ------------------------------------------------------------------ */
/* materials                                                           */

static void mat(s32 tex, s32 paint) {
    mb_flush();
    gDPPipeSync(gp++);
    if (paint) {
        gDPSetCombineLERP(gp++, TEXEL0, 0, SHADE, 0, 0, 0, 0, SHADE, COMBINED, 0, ENVIRONMENT, 0, 0, 0, 0, PRIMITIVE);
    } else {
        gDPSetCombineLERP(gp++, TEXEL0, 0, SHADE, 0, 0, 0, 0, SHADE, 0, 0, 0, COMBINED, 0, 0, 0, PRIMITIVE);
    }
    gDPLoadTextureBlock(gp++, nbTextures[tex], G_IM_FMT_RGBA, G_IM_SIZ_16b, NB_TEX_SIZE, NB_TEX_SIZE, 0,
                        G_TX_WRAP | G_TX_NOMIRROR, G_TX_WRAP | G_TX_NOMIRROR, 5, 5, G_TX_NOLOD, G_TX_NOLOD);
}

static void matFlat(s32 tintEnv) {
    mb_flush();
    gDPPipeSync(gp++);
    if (tintEnv) {
        gDPSetCombineLERP(gp++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, COMBINED, 0, ENVIRONMENT, 0, 0, 0, 0, PRIMITIVE);
    } else {
        gDPSetCombineLERP(gp++, 0, 0, 0, SHADE, 0, 0, 0, SHADE, 0, 0, 0, COMBINED, 0, 0, 0, PRIMITIVE);
    }
}

/* ------------------------------------------------------------------ */
/* primitives                                                          */

static f32 shade(const f32 n[3]) {
    f32 d = v3_dot(n, sLight);
    f32 b = 0.50f + 0.55f * (d > 0 ? d : 0);
    if (n[1] < -0.5f) b *= 0.8f;
    return b > 1.0f ? 1.0f : b;
}

/* convex polygon, auto-oriented away from the current centre hint */
static void mb_poly(f32 (*p)[3], f32 (*uv)[2], s32 n, s32 twoSided) {
    f32 e1[3], e2[3], nrm[3], cen[3] = {0, 0, 0}, hint[3];
    s32 i, idx[8], flip;
    f32 b;

    v3_sub(e1, p[1], p[0]);
    v3_sub(e2, p[2], p[0]);
    v3_cross(nrm, e1, e2);
    if (v3_normalize(nrm) < 1e-6f && n > 3) {
        v3_sub(e2, p[3], p[0]);
        v3_cross(nrm, e1, e2);
        v3_normalize(nrm);
    }
    for (i = 0; i < n; i++) v3_addScaled(cen, p[i], 1.0f / n);
    v3_sub(hint, cen, sCenter);
    flip = v3_dot(nrm, hint) < 0.0f;
    if (flip) v3_scale(nrm, nrm, -1.0f);
    b = shade(nrm);

    mb_reserve(n, (n - 2) * (twoSided ? 2 : 1));
    for (i = 0; i < n; i++) idx[i] = mb_vtx(p[i], uv[i][0], uv[i][1], b);
    for (i = 1; i < n - 1; i++) {
        if (flip) mb_tri(idx[0], idx[i + 1], idx[i]);
        else mb_tri(idx[0], idx[i], idx[i + 1]);
        if (twoSided) {
            if (flip) mb_tri(idx[0], idx[i], idx[i + 1]);
            else mb_tri(idx[0], idx[i + 1], idx[i]);
        }
    }
}

static void quad(f32 ax, f32 ay, f32 az, f32 bx, f32 by, f32 bz, f32 cx, f32 cy, f32 cz,
                 f32 dx, f32 dy, f32 dz, f32 u, f32 v, s32 two) {
    f32 p[4][3] = {{ax, ay, az}, {bx, by, bz}, {cx, cy, cz}, {dx, dy, dz}};
    f32 uv[4][2] = {{0, 0}, {u, 0}, {u, v}, {0, v}};
    mb_poly(p, uv, 4, two);
}

#define F_PX 0x01
#define F_NX 0x02
#define F_PY 0x04
#define F_NY 0x08
#define F_PZ 0x10
#define F_NZ 0x20
#define F_ALL 0x3F

/* axis-aligned box; texture repeats every `tile` units */
static void boxf(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1, f32 tile, s32 faces) {
    f32 dx = (x1 - x0) / tile, dy = (y1 - y0) / tile, dz = (z1 - z0) / tile;
    f32 old[3];
    v3_copy(old, sCenter);
    center((x0 + x1) * 0.5f, (y0 + y1) * 0.5f, (z0 + z1) * 0.5f);
    if (faces & F_PY) quad(x0, y1, z1, x1, y1, z1, x1, y1, z0, x0, y1, z0, dx, dz, 0);
    if (faces & F_NY) quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, dx, dz, 0);
    if (faces & F_PZ) quad(x0, y0, z1, x1, y0, z1, x1, y1, z1, x0, y1, z1, dx, dy, 0);
    if (faces & F_NZ) quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, dx, dy, 0);
    if (faces & F_PX) quad(x1, y0, z1, x1, y0, z0, x1, y1, z0, x1, y1, z1, dz, dy, 0);
    if (faces & F_NX) quad(x0, y0, z0, x0, y0, z1, x0, y1, z1, x0, y1, z0, dz, dy, 0);
    v3_copy(sCenter, old);
}

static void box(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1) {
    boxf(x0, y0, z0, x1, y1, z1, CELL, F_ALL);
}

/* basis for cylinders: axis 0=X, 1=Y, 2=Z */
static void basis(s32 axis, f32 a[3], f32 b1[3], f32 b2[3]) {
    v3_set(a, axis == 0, axis == 1, axis == 2);
    if (axis == 0) { v3_set(b1, 0, 1, 0); v3_set(b2, 0, 0, 1); }
    else if (axis == 1) { v3_set(b1, 0, 0, 1); v3_set(b2, 1, 0, 0); }
    else { v3_set(b1, 1, 0, 0); v3_set(b2, 0, 1, 0); }
}

static void ringPt(f32 out[3], const f32 c[3], const f32 a[3], const f32 b1[3], const f32 b2[3],
                   f32 h, f32 r, f32 ang) {
    f32 cs = cosf(ang) * r, sn = sinf(ang) * r;
    out[0] = c[0] + a[0] * h + b1[0] * cs + b2[0] * sn;
    out[1] = c[1] + a[1] * h + b1[1] * cs + b2[1] * sn;
    out[2] = c[2] + a[2] * h + b1[2] * cs + b2[2] * sn;
}

/* side of a (possibly tapered) cylinder from h0 (radius r0) to h1 (radius r1) */
static void cylSide(s32 axis, f32 cx, f32 cy, f32 cz, f32 r0, f32 r1, f32 h0, f32 h1, s32 segs, f32 urep, f32 vrep) {
    f32 a[3], b1[3], b2[3], c[3] = {cx, cy, cz}, old[3];
    s32 i;
    basis(axis, a, b1, b2);
    v3_copy(old, sCenter);
    v3_set(sCenter, cx + a[0] * (h0 + h1) * 0.5f, cy + a[1] * (h0 + h1) * 0.5f, cz + a[2] * (h0 + h1) * 0.5f);
    for (i = 0; i < segs; i++) {
        f32 p[4][3], uv[4][2];
        f32 t0 = (2 * NB_PI * i) / segs, t1 = (2 * NB_PI * (i + 1)) / segs;
        ringPt(p[0], c, a, b1, b2, h0, r0, t0);
        ringPt(p[1], c, a, b1, b2, h0, r0, t1);
        ringPt(p[2], c, a, b1, b2, h1, r1, t1);
        ringPt(p[3], c, a, b1, b2, h1, r1, t0);
        uv[0][0] = urep * i / segs;       uv[0][1] = 0;
        uv[1][0] = urep * (i + 1) / segs; uv[1][1] = 0;
        uv[2][0] = urep * (i + 1) / segs; uv[2][1] = vrep;
        uv[3][0] = urep * i / segs;       uv[3][1] = vrep;
        mb_poly(p, uv, 4, 0);
    }
    v3_copy(sCenter, old);
}

/* flat disc cap at height h, facing +axis if dir > 0 */
static void cylCap(s32 axis, f32 cx, f32 cy, f32 cz, f32 r, f32 h, s32 dir, s32 segs) {
    f32 a[3], b1[3], b2[3], c[3] = {cx, cy, cz}, old[3];
    s32 i;
    basis(axis, a, b1, b2);
    v3_copy(old, sCenter);
    v3_set(sCenter, cx + a[0] * (h - dir * 10.0f), cy + a[1] * (h - dir * 10.0f), cz + a[2] * (h - dir * 10.0f));
    for (i = 0; i < segs; i++) {
        f32 p[3][3], uv[3][2];
        f32 t0 = (2 * NB_PI * i) / segs, t1 = (2 * NB_PI * (i + 1)) / segs;
        ringPt(p[0], c, a, b1, b2, h, 0, 0);
        ringPt(p[1], c, a, b1, b2, h, r, t0);
        ringPt(p[2], c, a, b1, b2, h, r, t1);
        uv[0][0] = 0.5f; uv[0][1] = 0.5f;
        uv[1][0] = 0.5f + 0.5f * cosf(t0); uv[1][1] = 0.5f + 0.5f * sinf(t0);
        uv[2][0] = 0.5f + 0.5f * cosf(t1); uv[2][1] = 0.5f + 0.5f * sinf(t1);
        mb_poly(p, uv, 3, 0);
    }
    v3_copy(sCenter, old);
}

static void cylinder(s32 axis, f32 cx, f32 cy, f32 cz, f32 r, f32 h0, f32 h1, s32 segs) {
    cylSide(axis, cx, cy, cz, r, r, h0, h1, segs, 2.0f, (h1 - h0) / CELL);
    cylCap(axis, cx, cy, cz, r, h1, 1, segs);
    cylCap(axis, cx, cy, cz, r, h0, -1, segs);
}

static void sphere(f32 cx, f32 cy, f32 cz, f32 rx, f32 ry, f32 rz, s32 segs, s32 rings) {
    s32 i, j;
    f32 old[3];
    v3_copy(old, sCenter);
    center(cx, cy, cz);
    for (j = 0; j < rings; j++) {
        f32 p0 = -NB_PI / 2 + NB_PI * j / rings, p1 = -NB_PI / 2 + NB_PI * (j + 1) / rings;
        for (i = 0; i < segs; i++) {
            f32 t0 = 2 * NB_PI * i / segs, t1 = 2 * NB_PI * (i + 1) / segs;
            f32 p[4][3], uv[4][2];
            v3_set(p[0], cx + rx * cosf(p0) * cosf(t0), cy + ry * sinf(p0), cz + rz * cosf(p0) * sinf(t0));
            v3_set(p[1], cx + rx * cosf(p0) * cosf(t1), cy + ry * sinf(p0), cz + rz * cosf(p0) * sinf(t1));
            v3_set(p[2], cx + rx * cosf(p1) * cosf(t1), cy + ry * sinf(p1), cz + rz * cosf(p1) * sinf(t1));
            v3_set(p[3], cx + rx * cosf(p1) * cosf(t0), cy + ry * sinf(p1), cz + rz * cosf(p1) * sinf(t0));
            uv[0][0] = (f32)i / segs; uv[0][1] = (f32)j / rings;
            uv[1][0] = (f32)(i + 1) / segs; uv[1][1] = (f32)j / rings;
            uv[2][0] = (f32)(i + 1) / segs; uv[2][1] = (f32)(j + 1) / rings;
            uv[3][0] = (f32)i / segs; uv[3][1] = (f32)(j + 1) / rings;
            if (j == 0 || j == rings - 1) {
                /* poles: drop the degenerate corner */
                f32 tp[3][3], tuv[3][2];
                s32 k, m = 0;
                for (k = 0; k < 4; k++) {
                    if (j == 0 && k == 1) continue;
                    if (j == rings - 1 && k == 2) continue;
                    v3_copy(tp[m], p[k]);
                    tuv[m][0] = uv[k][0]; tuv[m][1] = uv[k][1];
                    m++;
                }
                mb_poly(tp, tuv, 3, 0);
            } else {
                mb_poly(p, uv, 4, 0);
            }
        }
    }
    v3_copy(sCenter, old);
}

/* wedge: full height at -Z, sloping down to the bottom at +Z */
static void wedge(f32 x0, f32 y0, f32 z0, f32 x1, f32 y1, f32 z1) {
    f32 old[3];
    v3_copy(old, sCenter);
    center((x0 + x1) * 0.5f, y0 + (y1 - y0) * 0.33f, z0 + (z1 - z0) * 0.33f);
    quad(x0, y0, z0, x1, y0, z0, x1, y0, z1, x0, y0, z1, 1, 1, 0);           /* bottom */
    quad(x1, y0, z0, x0, y0, z0, x0, y1, z0, x1, y1, z0, 1, 1, 0);           /* back */
    quad(x0, y1, z0, x1, y1, z0, x1, y0, z1, x0, y0, z1, 1, 1.41f, 0);       /* slope */
    {
        f32 p[3][3] = {{x1, y0, z0}, {x1, y0, z1}, {x1, y1, z0}};
        f32 uv[3][2] = {{0, 0}, {1, 0}, {0, 1}};
        mb_poly(p, uv, 3, 0);
    }
    {
        f32 p[3][3] = {{x0, y0, z0}, {x0, y0, z1}, {x0, y1, z0}};
        f32 uv[3][2] = {{0, 0}, {1, 0}, {0, 1}};
        mb_poly(p, uv, 3, 0);
    }
    v3_copy(sCenter, old);
}

/* ------------------------------------------------------------------ */
/* part meshes                                                         */

static void wheel(s32 type, f32 r, f32 halfw, s32 segs) {
    NBMesh *m = &nbMeshes[type];
    Gfx *s = mb_begin();
    m->body = NULL;
    mat(TEX_TIRE, 0);
    cylSide(0, 0, 0, 0, r, r, -halfw, halfw, segs, 4.0f, 1.0f);
    mat(TEX_HUB, 1);
    cylCap(0, 0, 0, 0, r, halfw, 1, segs);
    cylCap(0, 0, 0, 0, r, -halfw, -1, segs);
    m->spin = mb_end(s);
    v3_set(m->spinPivot, 0, 0, 0);
}

static void buildPart(s32 t) {
    NBMesh *m = &nbMeshes[t];
    Gfx *s;
    m->spin = NULL;
    v3_set(m->spinPivot, 0, 0, 0);

    switch (t) {
    case 0: /* wood block */
        s = mb_begin(); mat(TEX_WOOD, 1); box(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 1: /* metal block */
        s = mb_begin(); mat(TEX_METAL, 1); box(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 2:
        s = mb_begin(); mat(TEX_WOOD, 1); wedge(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 3:
        s = mb_begin(); mat(TEX_METAL, 1); wedge(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 4:
        s = mb_begin(); mat(TEX_WOOD, 1); boxf(-H, -H, -H, H, 0, H, CELL, F_ALL); m->body = mb_end(s); break;
    case 5: /* long plank */
        s = mb_begin(); mat(TEX_WOOD, 1); boxf(-H, -H, -3 * H, H, -H + 14, 3 * H, CELL, F_ALL); m->body = mb_end(s); break;
    case 6: /* metal girder: I-beam */
        s = mb_begin(); mat(TEX_METAL, 1);
        boxf(-H + 2, H - 6, -3 * H, H - 2, H, 3 * H, CELL, F_ALL);
        boxf(-H + 2, -H, -3 * H, H - 2, -H + 6, 3 * H, CELL, F_ALL);
        boxf(-5, -H + 6, -3 * H, 5, H - 6, 3 * H, CELL, F_PX | F_NX | F_PZ | F_NZ);
        m->body = mb_end(s); break;
    case 7:
        s = mb_begin(); mat(TEX_GLASS, 0); box(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 8:
        s = mb_begin(); mat(TEX_HONEYCOMB, 0); box(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 9:
        s = mb_begin(); mat(TEX_JIGGY, 0); box(-H, -H, -H, H, H, H); m->body = mb_end(s); break;
    case 10: /* rubber bumper */
        s = mb_begin(); mat(TEX_TIRE, 0); box(-H + 1, -H + 3, -H + 1, H - 1, H - 3, H - 1);
        mat(TEX_METAL, 0); boxf(-H, -4, -H, H, 4, H, CELL, F_ALL);
        m->body = mb_end(s); break;

    case 11: wheel(t, nbPartDefs[t].value, 12.0f, 12); break;
    case 12: wheel(t, nbPartDefs[t].value, 16.0f, 14); break;
    case 13: wheel(t, nbPartDefs[t].value, 18.0f, 16); break;
    case 14: /* ski */
        s = mb_begin(); mat(TEX_PLAIN, 1);
        boxf(-12, -H, -2 * H, 12, -H + 5, 2 * H - 10, CELL, F_ALL);
        wedge(-12, -H, 2 * H - 10, 12, -H + 12, 2 * H);
        mat(TEX_METAL, 0);
        boxf(-4, -H + 5, -10, 4, H, 10, CELL, F_ALL);
        m->body = mb_end(s); break;
    case 15: /* tank tread */
        s = mb_begin(); mat(TEX_TIRE, 0);
        boxf(-16, -H, -3 * H + 20, 16, H, 3 * H - 20, 20.0f, F_ALL);
        cylSide(0, 0, 0, 3 * H - 20, H, H, -16, 16, 8, 2, 1);
        cylSide(0, 0, 0, -3 * H + 20, H, H, -16, 16, 8, 2, 1);
        mat(TEX_HUB, 0);
        cylCap(0, 17, 0, 3 * H - 20, H - 4, 0, 1, 8);
        cylCap(0, 17, 0, -3 * H + 20, H - 4, 0, 1, 8);
        cylCap(0, -17, 0, 3 * H - 20, H - 4, 0, -1, 8);
        cylCap(0, -17, 0, -3 * H + 20, H - 4, 0, -1, 8);
        m->body = mb_end(s); break;

    case 16: /* small engine */
        s = mb_begin(); mat(TEX_ENGINE, 0);
        box(-H + 2, -H, -H + 2, H - 2, H - 8, H - 2);
        mat(TEX_METAL, 0);
        cylinder(1, -8, H - 8, 0, 6, 0, 10, 8);
        cylinder(1, 8, H - 8, 0, 6, 0, 10, 8);
        m->body = mb_end(s); break;
    case 17: /* big engine */
        s = mb_begin(); mat(TEX_ENGINE, 0);
        box(-CELL + 2, -H, -CELL + 2, CELL - 2, H - 6, CELL - 2);
        mat(TEX_GRILLE, 0);
        boxf(-CELL + 8, H - 6, -CELL + 8, CELL - 8, H - 2, CELL - 8, CELL, F_PY);
        mat(TEX_METAL, 0);
        cylinder(1, -18, H - 6, -14, 7, 0, 12, 8);
        cylinder(1, 18, H - 6, -14, 7, 0, 12, 8);
        cylinder(1, -18, H - 6, 14, 7, 0, 12, 8);
        cylinder(1, 18, H - 6, 14, 7, 0, 12, 8);
        m->body = mb_end(s); break;
    case 18: /* fuel can */
        s = mb_begin(); mat(TEX_FUEL, 1);
        box(-12, -H, -16, 12, H - 6, 16);
        mat(TEX_METAL, 0);
        cylinder(1, 6, H - 6, 10, 4, 0, 6, 6);
        boxf(-8, H - 6, -12, 2, H - 2, -4, CELL, F_ALL);
        m->body = mb_end(s); break;
    case 19: /* fuel tank */
        s = mb_begin(); mat(TEX_FUEL, 1);
        cylSide(2, 0, -2, 0, 17, 17, -CELL + 4, CELL - 4, 12, 2, 2);
        mat(TEX_METAL, 0);
        cylCap(2, 0, -2, 0, 17, CELL - 4, 1, 12);
        cylCap(2, 0, -2, 0, 17, -CELL + 4, -1, 12);
        boxf(-14, -H, -CELL + 10, 14, -H + 4, CELL - 10, CELL, F_ALL);
        m->body = mb_end(s); break;
    case 20: /* seat */
        s = mb_begin(); mat(TEX_SEAT, 1);
        box(-H + 2, -H, -H + 2, H - 2, -6, H - 2);
        box(-H + 2, -6, -H + 2, H - 2, H + 12, -H + 9);
        mat(TEX_METAL, 0);
        boxf(-H, -6, -H + 2, -H + 4, 4, H - 4, CELL, F_ALL);
        boxf(H - 4, -6, -H + 2, H, 4, H - 4, CELL, F_ALL);
        m->body = mb_end(s); break;

    case 21: /* propeller: mount + spinning blades around Z */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-8, -8, -H, 8, 8, 4, CELL, F_ALL);
        cylSide(2, 0, 0, 0, 9, 3, 4, 16, 8, 1, 0.3f);
        m->body = mb_end(s);
        s = mb_begin(); mat(TEX_WOOD, 0);
        color(230, 210, 170);
        boxf(-4, 6, 8, 4, H + 18, 11, CELL, F_ALL);
        boxf(-4, -H - 18, 8, 4, -6, 11, CELL, F_ALL);
        boxf(6, -4, 8, H + 18, 4, 11, CELL, F_ALL);
        boxf(-H - 18, -4, 8, -6, 4, 11, CELL, F_ALL);
        m->spin = mb_end(s);
        break;
    case 22: /* jet booster: nozzle at the back (-Z) */
        s = mb_begin(); mat(TEX_METAL, 0);
        cylSide(2, 0, 0, 0, 12, 15, 10, CELL - 4, 12, 2, 1);
        cylSide(2, 0, 0, 0, 15, 15, -CELL + 10, 10, 12, 2, 1);
        cylCap(2, 0, 0, 0, 12, CELL - 4, 1, 12);
        mat(TEX_JET, 0);
        cylCap(2, 0, 0, 0, 15, -CELL + 10, -1, 12);
        mat(TEX_HAZARD, 0);
        cylSide(2, 0, 0, 0, 15.5f, 15.5f, -6, 2, 12, 2, 0.2f);
        m->body = mb_end(s); break;
    case 23: /* wing: 3x2 thin panel, tapered */
        s = mb_begin(); mat(TEX_CANVAS, 1);
        {
            f32 xw = 3 * H, zf = CELL - 2, zb = -CELL + 2;
            f32 p[4][3], uv[4][2];
            center(0, 0, 0);
            v3_set(p[0], -xw, 3, zb); v3_set(p[1], xw, 3, zb + 10); v3_set(p[2], xw, 3, zf - 18); v3_set(p[3], -xw, 3, zf);
            uv[0][0] = 0; uv[0][1] = 0; uv[1][0] = 3; uv[1][1] = 0.25f; uv[2][0] = 3; uv[2][1] = 1.55f; uv[3][0] = 0; uv[3][1] = 2;
            mb_poly(p, uv, 4, 0);
            v3_set(p[0], -xw, -3, zb); v3_set(p[1], xw, -3, zb + 10); v3_set(p[2], xw, -3, zf - 18); v3_set(p[3], -xw, -3, zf);
            mb_poly(p, uv, 4, 0);
            mat(TEX_METAL, 1);
            boxf(-xw, -3, zf - 4, xw, 3, zf, CELL, F_PZ | F_PY | F_NY);
            boxf(-xw, -4, -6, xw, 4, 6, CELL, F_PY | F_NY);
        }
        m->body = mb_end(s); break;
    case 24: /* tail fin */
        s = mb_begin(); mat(TEX_CANVAS, 1);
        {
            f32 p[4][3], uv[4][2] = {{0, 0}, {1, 0}, {0.6f, 1}, {0, 1}};
            center(0, 0, 0);
            v3_set(p[0], 3, -H, -H); v3_set(p[1], 3, -H, H); v3_set(p[2], 3, H, -2); v3_set(p[3], 3, H, -H);
            mb_poly(p, uv, 4, 0);
            v3_set(p[0], -3, -H, -H); v3_set(p[1], -3, -H, H); v3_set(p[2], -3, H, -2); v3_set(p[3], -3, H, -H);
            mb_poly(p, uv, 4, 0);
            boxf(-3, -H, -H, 3, H, -H + 3, CELL, F_NZ);
        }
        m->body = mb_end(s); break;
    case 25: /* floater */
        s = mb_begin(); mat(TEX_FLOATER, 0);
        cylSide(1, 0, 0, 0, 16, 19, -H + 2, 0, 12, 2, 0.5f);
        cylSide(1, 0, 0, 0, 19, 16, 0, H - 2, 12, 2, 0.5f);
        cylCap(1, 0, 0, 0, 16, H - 2, 1, 12);
        cylCap(1, 0, 0, 0, 16, -H + 2, -1, 12);
        m->body = mb_end(s); break;
    case 26: /* balloon: 1x2x1, string at the bottom */
        s = mb_begin(); mat(TEX_BALLOON, 1);
        sphere(0, 10, 0, 22, 26, 22, 10, 6);
        mat(TEX_PLAIN, 0); color(200, 200, 200);
        boxf(-1, -CELL, -1, 1, -14, 1, CELL, F_PX | F_NX | F_PZ | F_NZ);
        m->body = mb_end(s); break;

    case 27: /* shock spring pad */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-H + 2, -H, -H + 2, H - 2, -H + 5, H - 2, CELL, F_ALL);
        mat(TEX_HAZARD, 0);
        cylSide(1, 0, 0, 0, 10, 12, -H + 5, H - 8, 8, 3, 1);
        mat(TEX_HAZARD, 0);
        boxf(-H + 4, H - 8, -H + 4, H - 4, H - 3, H - 4, CELL, F_ALL);
        m->body = mb_end(s); break;
    case 28: /* egg cannon */
        s = mb_begin(); mat(TEX_METAL, 1);
        boxf(-16, -H, -CELL + 4, 16, -6, 0, CELL, F_ALL);
        cylSide(2, 0, 4, 0, 11, 11, -CELL + 8, CELL - 2, 10, 2, 2);
        cylCap(2, 0, 4, 0, 11, -CELL + 8, -1, 10);
        mat(TEX_PLAIN, 0); color(40, 40, 40);
        cylCap(2, 0, 4, 0, 8, CELL - 1, 1, 10);
        color(255, 255, 255);
        mat(TEX_METAL, 0);
        cylSide(2, 0, 4, 0, 13, 13, CELL - 10, CELL - 2, 10, 2, 0.3f);
        m->body = mb_end(s); break;
    case 29: /* horn */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-8, -H, -10, 8, -H + 8, 10, CELL, F_ALL);
        mat(TEX_JIGGY, 0);
        cylSide(2, 0, -6, 0, 3, 3, -14, 6, 8, 1, 1);
        cylSide(2, 0, -6, 0, 3, 13, 6, 18, 8, 1, 1);
        cylCap(2, 0, -6, 0, 13, 18, 1, 8);
        m->body = mb_end(s); break;
    case 30: /* headlight */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-12, -H, -8, 12, 4, 8, CELL, F_ALL);
        cylSide(2, 0, 4, 0, 12, 12, -6, 12, 10, 1, 0.5f);
        mat(TEX_LAMP, 0);
        cylCap(2, 0, 4, 0, 12, 12, 1, 10);
        m->body = mb_end(s); break;

    case 31: /* mumbo skull */
        s = mb_begin(); mat(TEX_PLAIN, 0); color(240, 236, 220);
        boxf(-16, -H, -16, 16, H - 6, 16, CELL, F_ALL & ~F_PZ);
        color(255, 255, 255); mat(TEX_SKULL, 0);
        boxf(-16, -H, -16, 16, H - 6, 16, 32.0f, F_PZ);
        mat(TEX_PLAIN, 0); color(220, 60, 50);
        boxf(-3, H - 6, -4, 3, H + 14, 4, CELL, F_ALL);
        m->body = mb_end(s); break;
    case 32: /* music note sign */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-3, -H, -3, 3, -2, 3, CELL, F_ALL);
        mat(TEX_NOTE, 0);
        boxf(-H + 2, -2, -2, H - 2, H, 2, 36.0f, F_PZ | F_NZ);
        mat(TEX_JIGGY, 0);
        boxf(-H + 2, -2, -2, H - 2, H, 2, CELL, F_PX | F_NX | F_PY | F_NY);
        m->body = mb_end(s); break;
    case 33: /* checker flag (1x2x1) */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-14, -CELL, -2, -10, CELL - 2, 2, CELL, F_ALL);
        mat(TEX_CHECKER, 0);
        {
            f32 p[4][3], uv[4][2] = {{0, 0}, {1, 0}, {1, 0.6f}, {0, 0.6f}};
            center(0, 0, 0);
            v3_set(p[0], -10, 8, 0); v3_set(p[1], 18, 10, 0); v3_set(p[2], 18, CELL - 4, 0); v3_set(p[3], -10, CELL - 2, 0);
            mb_poly(p, uv, 4, 1);
        }
        m->body = mb_end(s); break;
    case 34: /* exhaust pipe */
        s = mb_begin(); mat(TEX_METAL, 0);
        boxf(-10, -H, -10, 10, -H + 6, 10, CELL, F_ALL);
        cylSide(1, 0, 0, 0, 6, 7, -H + 6, H + 8, 8, 1, 1);
        mat(TEX_PLAIN, 0); color(30, 30, 30);
        cylCap(1, 0, 0, 0, 7, H + 8, 1, 8);
        m->body = mb_end(s); break;
    case 35: /* front grille */
        s = mb_begin(); mat(TEX_METAL, 1);
        boxf(-H, -H, -H, H, H, H - 6, CELL, F_ALL & ~F_PZ);
        mat(TEX_GRILLE, 0);
        boxf(-H, -H, H - 6, H, H, H, CELL, F_PZ);
        mat(TEX_METAL, 1);
        boxf(-H, -H, H - 6, H, H, H, CELL, F_PX | F_NX | F_PY | F_NY);
        m->body = mb_end(s); break;
    default:
        m->body = NULL;
        break;
    }
}

/* ------------------------------------------------------------------ */
/* garage helpers                                                      */

static void buildGrid(void) {
    Gfx *s = mb_begin();
    s32 i;
    f32 e = (GRID_HALF + 0.5f) * CELL;
    matFlat(0);
    color(200, 230, 255);
    for (i = -GRID_HALF; i <= GRID_HALF + 1; i++) {
        f32 c = (i - 0.5f) * CELL;
        boxf(c - 1.0f, 0.5f, -e, c + 1.0f, 1.5f, e, CELL, F_PY);
        boxf(-e, 0.5f, c - 1.0f, e, 1.5f, c + 1.0f, CELL, F_PY);
    }
    nbGridDL = mb_end(s);
}

/* unit cube wireframe, -0.5..0.5 cells */
static void buildCursor(void) {
    Gfx *s = mb_begin();
    f32 h = H + 1.0f, t = 1.5f;
    matFlat(1);
    color(255, 255, 255);
    /* 4 vertical edges */
    boxf(-h - t, -h, -h - t, -h + t, h, -h + t, CELL, F_ALL);
    boxf(h - t, -h, -h - t, h + t, h, -h + t, CELL, F_ALL);
    boxf(-h - t, -h, h - t, -h + t, h, h + t, CELL, F_ALL);
    boxf(h - t, -h, h - t, h + t, h, h + t, CELL, F_ALL);
    /* top & bottom rings */
    boxf(-h, h - t, -h - t, h, h + t, -h + t, CELL, F_ALL);
    boxf(-h, h - t, h - t, h, h + t, h + t, CELL, F_ALL);
    boxf(-h - t, h - t, -h, -h + t, h + t, h, CELL, F_ALL);
    boxf(h - t, h - t, -h, h + t, h + t, h, CELL, F_ALL);
    boxf(-h, -h - t, -h - t, h, -h + t, -h + t, CELL, F_ALL);
    boxf(-h, -h - t, h - t, h, -h + t, h + t, CELL, F_ALL);
    boxf(-h - t, -h - t, -h, -h + t, -h + t, h, CELL, F_ALL);
    boxf(h - t, -h - t, -h, h + t, -h + t, h, CELL, F_ALL);
    nbCursorDL = mb_end(s);
}

static void buildPad(void) {
    Gfx *s = mb_begin();
    f32 e = (GRID_HALF + 0.5f) * CELL + 12.0f;
    mat(TEX_CHECKER, 0);
    color(200, 200, 200);
    boxf(-e, -10.0f, -e, e, 0.0f, e, CELL * 2, F_PY);
    mat(TEX_METAL, 0);
    color(255, 220, 120);
    boxf(-e, -10.0f, -e, e, 0.0f, e, 20.0f, F_PX | F_NX | F_PZ | F_NZ);
    nbPadDL = mb_end(s);
}

static void buildShadow(void) {
    Gfx *s = mb_begin();
    s32 i, n = 12;
    matFlat(0);
    color(0, 0, 0);
    for (i = 0; i < n; i++) {
        f32 p[3][3], uv[3][2] = {{0, 0}, {0, 0}, {0, 0}};
        f32 a0 = 2 * NB_PI * i / n, a1 = 2 * NB_PI * (i + 1) / n;
        center(0, -10, 0);
        v3_set(p[0], 0, 0, 0);
        v3_set(p[1], cosf(a0) * 0.5f * CELL, 0, sinf(a0) * 0.5f * CELL);
        v3_set(p[2], cosf(a1) * 0.5f * CELL, 0, sinf(a1) * 0.5f * CELL);
        mb_poly(p, uv, 3, 0);
    }
    nbShadowDL = mb_end(s);
}

void mesh_buildAll(void) {
    s32 i;
    v3_set(sLight, 0.35f, 0.85f, 0.40f);
    v3_normalize(sLight);
    sVtxUsed = sGfxUsed = 0;
    for (i = 0; i < nbPartCount; i++) buildPart(i);
    buildGrid();
    buildCursor();
    buildPad();
    buildShadow();
    osWritebackDCache(sVtx, sizeof(sVtx));
    osWritebackDCache(sGfx, sizeof(sGfx));
}
