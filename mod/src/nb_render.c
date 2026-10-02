/*
 * Per-frame rendering. Everything is emitted into the mod's own display list
 * and matrix buffers (in Expansion Pak RAM); the game's display list only gets
 * a single gSPDisplayList call, so the game's small gfx buffers aren't touched.
 */
#include "nb.h"

#define FRAME_BUFS 3
#define FRAME_GFX  4096
#define FRAME_MTX  520

static Gfx sFrameGfx[FRAME_BUFS][FRAME_GFX] __attribute__((aligned(8)));
static Mtx sFrameMtx[FRAME_BUFS][FRAME_MTX] __attribute__((aligned(16)));
static Mtx sIdentity __attribute__((aligned(16)));
static s32 sFrame, sIdentityInit;
static Gfx *fg, *fgEnd;
static Mtx *fm, *fmEnd;
static f32 sCam[3];

typedef struct {
    f32 ax[3][3];   /* blueprint-local axes in world space */
    f32 origin[3];  /* world position of blueprint-local (0,0,0) */
} Frame;

/* ------------------------------------------------------------------ */

static s32 room(s32 gfxCmds) {
    return (fg + gfxCmds < fgEnd - 24) && (fm < fmEnd - 2);
}

static void emitMatrix(f32 rows[3][3], const f32 worldPos[3]) {
    f32 t[3];
    v3_sub(t, worldPos, sCam);
    mtx_fromAxes(fm, rows, t);
    gSPMatrix(fg++, fm, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    fm++;
}

static void partAxes(s32 rot, f32 steerDeg, f32 out[3][3]) {
    static const f32 R[4][3] = {{1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 1}};
    static const f32 F[4][3] = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}};
    v3_copy(out[0], R[rot & 3]);
    v3_set(out[1], 0, 1, 0);
    v3_copy(out[2], F[rot & 3]);
    if (steerDeg != 0.0f) {
        v3_rotateYaw(out[0], out[0], steerDeg);
        v3_rotateYaw(out[2], out[2], steerDeg);
    }
}

/* local rows (in blueprint space) -> world rows through the frame */
static void toWorldRows(const Frame *f, f32 local[3][3], f32 out[3][3]) {
    s32 i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++)
            out[i][j] = local[i][0] * f->ax[0][j] + local[i][1] * f->ax[1][j] + local[i][2] * f->ax[2][j];
}

static void frameToWorld(const Frame *f, const f32 l[3], f32 out[3]) {
    s32 j;
    for (j = 0; j < 3; j++)
        out[j] = f->origin[j] + l[0] * f->ax[0][j] + l[1] * f->ax[1][j] + l[2] * f->ax[2][j];
}

static void drawPartAt(const Frame *f, s32 type, s32 rot, s32 color, const f32 center[3],
                       f32 spin, f32 steerDeg, f32 lift, u8 alpha, f32 scale) {
    const NBMesh *m = &nbMeshes[type];
    const NBPartDef *d = &nbPartDefs[type];
    f32 local[3][3], rows[3][3], pos[3], c[3];
    s32 i;

    if (!room(8)) return;
    v3_copy(c, center);
    c[1] += lift;
    frameToWorld(f, c, pos);
    partAxes(rot, steerDeg, local);
    toWorldRows(f, local, rows);
    if (scale != 1.0f)
        for (i = 0; i < 3; i++) v3_scale(rows[i], rows[i], scale);

    gDPSetEnvColor(fg++, nbPalette[color][0], nbPalette[color][1], nbPalette[color][2], 255);
    gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, alpha);
    if (m->body) {
        emitMatrix(rows, pos);
        gSPDisplayList(fg++, m->body);
    }
    if (m->spin) {
        f32 sr[3][3], cs = cosf(spin), sn = sinf(spin);
        if (d->flags & PF_SPIN_X) {
            v3_copy(sr[0], rows[0]);
            for (i = 0; i < 3; i++) {
                sr[1][i] = cs * rows[1][i] + sn * rows[2][i];
                sr[2][i] = -sn * rows[1][i] + cs * rows[2][i];
            }
        } else {
            v3_copy(sr[2], rows[2]);
            for (i = 0; i < 3; i++) {
                sr[0][i] = cs * rows[0][i] + sn * rows[1][i];
                sr[1][i] = -sn * rows[0][i] + cs * rows[1][i];
            }
        }
        emitMatrix(sr, pos);
        gSPDisplayList(fg++, m->spin);
    }
}

static void setOpaque(void) {
    gDPPipeSync(fg++);
    gDPSetRenderMode(fg++, G_RM_PASS, G_RM_AA_ZB_OPA_SURF2);
}

static void setXlu(void) {
    gDPPipeSync(fg++);
    gDPSetRenderMode(fg++, G_RM_PASS, G_RM_AA_ZB_XLU_SURF2);
}

static void beginFrame(void) {
    gDPPipeSync(fg++);
    gDPPipelineMode(fg++, G_PM_1PRIMITIVE);
    gDPSetCycleType(fg++, G_CYC_2CYCLE);
    gSPClearGeometryMode(fg++, G_LIGHTING | G_TEXTURE_GEN | G_TEXTURE_GEN_LINEAR | G_CULL_BOTH);
    gSPSetGeometryMode(fg++, G_ZBUFFER | G_SHADE | G_SHADING_SMOOTH | G_CULL_BACK);
    gSPTexture(fg++, 0xFFFF, 0xFFFF, 0, G_TX_RENDERTILE, G_ON);
    gDPSetTextureLOD(fg++, G_TL_TILE);
    gDPSetTextureLUT(fg++, G_TT_NONE);
    gDPSetTextureDetail(fg++, G_TD_CLAMP);
    gDPSetTexturePersp(fg++, G_TP_PERSP);
    gDPSetTextureFilter(fg++, G_TF_BILERP);
    gDPSetTextureConvert(fg++, G_TC_FILT);
    gDPSetCombineKey(fg++, G_CK_NONE);
    gDPSetAlphaCompare(fg++, G_AC_NONE);
    gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, 255);
    setOpaque();
}

static void endFrame(void) {
    gDPPipeSync(fg++);
    gDPSetCycleType(fg++, G_CYC_2CYCLE);
    gDPPipelineMode(fg++, G_PM_1PRIMITIVE);
    gDPSetEnvColor(fg++, 255, 255, 255, 255);
    gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, 255);
    gDPSetBlendColor(fg++, 0, 0, 0, 0x80);
    gDPSetTextureLOD(fg++, G_TL_TILE);
    gSPMatrix(fg++, &sIdentity, G_MTX_MODELVIEW | G_MTX_LOAD | G_MTX_NOPUSH);
    gSPEndDisplayList(fg++);
}

/* ------------------------------------------------------------------ */

static void garageFrame(Frame *f) {
    f32 fwd[3] = {0, 0, 1}, left[3] = {1, 0, 0};
    v3_rotateYaw(f->ax[2], fwd, nbGarage.yaw);
    v3_rotateYaw(f->ax[0], left, nbGarage.yaw);
    v3_set(f->ax[1], 0, 1, 0);
    v3_copy(f->origin, nbGarage.origin);
}

static void vehicleFrame(Frame *f) {
    f32 cmw[3];
    s32 i, j;
    for (i = 0; i < 3; i++)
        for (j = 0; j < 3; j++) f->ax[i][j] = nbVeh.ax[i][j];
    for (j = 0; j < 3; j++)
        cmw[j] = nbVeh.cm[0] * f->ax[0][j] + nbVeh.cm[1] * f->ax[1][j] + nbVeh.cm[2] * f->ax[2][j];
    v3_sub(f->origin, nbVeh.x, cmw);
}

static void drawBlueprint(const Frame *f, s32 inVehicle) {
    s32 i;
    for (i = 0; i < nbBlueprint.count; i++) {
        const NBPart *p = &nbBlueprint.parts[i];
        const NBPartDef *d = &nbPartDefs[p->type];
        f32 c[3], spin = 0, steer = 0, lift = 0;
        if (inVehicle && nbVeh.broken[i]) continue;
        bp_partCenter(p, c);
        if (inVehicle) {
            s32 w;
            if (d->flags & PF_SPIN_Z) spin = nbVeh.spinAngle;
            if (d->kind == KIND_WHEEL) {
                for (w = 0; w < nbVeh.numWheels; w++) {
                    if (nbVeh.wheels[w].part == i) {
                        spin = nbVeh.wheels[w].spin;
                        lift = nb_clampf(nbVeh.wheels[w].compression, -20.0f, 20.0f);
                        if (nbVeh.wheels[w].steer) steer = -nbVeh.steer * 32.0f;
                        break;
                    }
                }
            }
        }
        drawPartAt(f, p->type, p->rot, p->color, c, spin, steer, lift, 255, 1.0f);
    }
}

static void drawShadow(void) {
    f32 start[3], end[3], n[3], rows[3][3], pos[3], s;
    f32 fwd[3] = {0, 0, 1}, left[3] = {1, 0, 0};
    v3_copy(start, nbVeh.x);
    v3_copy(end, nbVeh.x);
    end[1] -= 1500.0f;
    if (func_80320B98(start, end, n, NB_FLOOR_FLAGS) == NULL) return;
    if (!room(8)) return;
    s = (nbVeh.halfExtent[0] + nbVeh.halfExtent[2]) / CELL * 1.2f;
    s *= nb_clampf(1.0f - (nbVeh.x[1] - end[1]) / 1500.0f, 0.3f, 1.0f);
    v3_rotateYaw(rows[2], fwd, vehicle_yaw(&nbVeh));
    v3_rotateYaw(rows[0], left, vehicle_yaw(&nbVeh));
    v3_set(rows[1], 0, 1, 0);
    v3_scale(rows[0], rows[0], s);
    v3_scale(rows[2], rows[2], s);
    v3_set(pos, end[0], end[1] + 2.0f, end[2]);
    gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, 110);
    emitMatrix(rows, pos);
    gSPDisplayList(fg++, nbShadowDL);
}

static void drawGarage(void) {
    NBGarage *g = &nbGarage;
    Frame f;
    f32 rows[3][3], pos[3], c[3];
    s32 sx, sy, sz, ok;
    NBPart ghost;
    u8 a;

    garageFrame(&f);

    setOpaque();
    /* the vehicle so far */
    drawBlueprint(&f, FALSE);

    setXlu();
    /* floor grid on the cursor's layer */
    {
        f32 o[3];
        v3_set(c, 0, g->cy * CELL, 0);
        frameToWorld(&f, c, o);
        gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, 90);
        emitMatrix(f.ax, o);
        gSPDisplayList(fg++, nbGridDL);
    }

    /* ghost of the selected part */
    ghost.type = g->type; ghost.rot = g->rot; ghost.x = g->cx; ghost.y = g->cy; ghost.z = g->cz;
    bp_partExtent(&ghost, &sx, &sy, &sz);
    bp_partCenter(&ghost, c);
    {
        /* placement check is O(parts x cells): only redo it when something changed */
        static u32 lastRev = 0xFFFFFFFF;
        static s32 lastKey = -1, lastOk;
        s32 key = (((g->cx + 8) * 16 + (g->cz + 8)) * 16 + g->cy) * 256 + g->type * 4 + g->rot;
        if (key != lastKey || lastRev != nbBlueprintRev) {
            lastOk = nb_partUnlocked(g->type) && bp_canPlace(&nbBlueprint, g->type, g->cx, g->cy, g->cz, g->rot) == 1;
            lastKey = key;
            lastRev = nbBlueprintRev;
        }
        ok = lastOk;
    }
    a = (u8)(120 + 60 * sinf(g->blink * 6.0f));
    drawPartAt(&f, g->type, g->rot, g->color, c, g->blink * 3.0f, 0, 0, a, 1.0f);

    /* cursor box scaled to the footprint, green if it fits */
    {
        f32 local[3][3];
        partAxes(0, 0, local);
        v3_scale(local[0], local[0], (f32)sx);
        v3_scale(local[1], local[1], (f32)sy);
        v3_scale(local[2], local[2], (f32)sz);
        toWorldRows(&f, local, rows);
        frameToWorld(&f, c, pos);
        if (ok) {
            gDPSetEnvColor(fg++, 90, 255, 110, 255);
        } else {
            gDPSetEnvColor(fg++, 255, 80, 60, 255);
        }
        gDPSetPrimColor(fg++, 0, 0, 255, 255, 255, 220);
        emitMatrix(rows, pos);
        gSPDisplayList(fg++, nbCursorDL);
    }
    setOpaque();
}

static void drawDebris(void) {
    s32 i;
    Frame f;
    for (i = 0; i < MAX_DEBRIS; i++) {
        NBDebris *d = &nbDebris[i];
        f32 zero[3] = {0, 0, 0};
        s32 j, k;
        if (!d->active) continue;
        for (j = 0; j < 3; j++)
            for (k = 0; k < 3; k++) f.ax[j][k] = d->ax[j][k];
        v3_copy(f.origin, d->pos);
        drawPartAt(&f, d->type, d->rot, d->color, zero, 0, 0, 0, 255, 1.0f);
    }
}

void render_world(Gfx **gfx, Mtx **mtx, Vtx **vtx) {
    s32 anyDebris = FALSE, i;

    for (i = 0; i < MAX_DEBRIS; i++) anyDebris |= nbDebris[i].active;
    if (!nbGarage.active && !nbVeh.active && !anyDebris) return;

    if (!sIdentityInit) {
        f32 id[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}, z[3] = {0, 0, 0};
        mtx_fromAxes(&sIdentity, id, z);
        osWritebackDCache(&sIdentity, sizeof(Mtx));
        sIdentityInit = TRUE;
    }

    sFrame = (sFrame + 1) % FRAME_BUFS;
    fg = sFrameGfx[sFrame];
    fgEnd = fg + FRAME_GFX;
    fm = sFrameMtx[sFrame];
    fmEnd = fm + FRAME_MTX;
    viewport_getPosition_vec3f(sCam);

    beginFrame();
    if (nbGarage.active) drawGarage();
    if (nbVeh.active) {
        Frame f;
        vehicleFrame(&f);
        drawBlueprint(&f, TRUE);
        setXlu();
        drawShadow();
        setOpaque();
    }
    if (anyDebris) drawDebris();
    endFrame();

    osWritebackDCache(sFrameGfx[sFrame], (u8 *)fg - (u8 *)sFrameGfx[sFrame]);
    osWritebackDCache(sFrameMtx[sFrame], (u8 *)fm - (u8 *)sFrameMtx[sFrame]);
    gSPDisplayList((*gfx)++, sFrameGfx[sFrame]);
}

/* ------------------------------------------------------------------ */
/* HUD                                                                 */

#define MAX_TEXT 16
static char sText[MAX_TEXT][40];
static s16 sTextX[MAX_TEXT], sTextY[MAX_TEXT];
static u8 sTextC[MAX_TEXT][3];
static s32 sTextN;

typedef struct { s16 x, y, w, h; u8 r, g, b, a; } Rect;
static Rect sRects[16];
static s32 sRectN;

void hud_queueText(s32 x, s32 y, const char *str, u8 r, u8 g, u8 b) {
    s32 i;
    if (sTextN >= MAX_TEXT) return;
    for (i = 0; i < 39 && str[i]; i++) {
        char ch = str[i];
        if (ch >= 'a' && ch <= 'z') ch -= 32;
        sText[sTextN][i] = ch;
    }
    sText[sTextN][i] = 0;
    sTextX[sTextN] = x;
    sTextY[sTextN] = y;
    sTextC[sTextN][0] = r; sTextC[sTextN][1] = g; sTextC[sTextN][2] = b;
    sTextN++;
}

void hud_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b, u8 a) {
    Rect *rc;
    if (sRectN >= 16) return;
    rc = &sRects[sRectN++];
    rc->x = x; rc->y = y; rc->w = w; rc->h = h;
    rc->r = r; rc->g = g; rc->b = b; rc->a = a;
}

static void driveHud(void) {
    NBVehicle *v = &nbVeh;
    static char speed[24], tip[2][40];
    static f32 tipTimer;
    char num[12];

    if (nbMode != MODE_DRIVE) {
        tipTimer = 0;
        return;
    }
    tipTimer += 1.0f / 30.0f;

    nb_strcpy(speed, "SPEED ");
    nb_itoa(num, (s32)(v->speed / 20.0f));
    nb_strcat(speed, num);
    hud_queueText(200, 196, speed, 255, 255, 255);

    if (v->fuelMax > 0) {
        f32 frac = v->fuel / v->fuelMax;
        hud_rect(14, 192, 104, 10, 20, 20, 30, 170);
        hud_rect(16, 194, (s32)(100 * frac), 6, frac < 0.25f ? 255 : 250, frac < 0.25f ? 70 : 170, 40, 255);
        hud_queueText(16, 180, v->fuel <= 0 ? "OUT OF FUEL!" : "FUEL", 255, 220, 120);
    }
    if (tipTimer < 6.0f) {
        nb_strcpy(tip[0], "A GAS  B BRAKE  L GET OUT");
        nb_strcpy(tip[1], v->numJets ? "Z BOOST  R FIRE, HORN  D-UP FLIP" : "R FIRE, HORN  C-UP JUMP  D-UP FLIP");
        hud_queueText(16, 16, tip[0], 200, 230, 255);
        hud_queueText(16, 30, tip[1], 200, 230, 255);
    }
}

void hud_flushText(void) {
    s32 i;
    driveHud();
    for (i = 0; i < sTextN; i++) {
        text_setNormalTextColor(sTextC[i][0], sTextC[i][1], sTextC[i][2]);
        print_dialog(sTextX[i], sTextY[i], (u8 *)sText[i]);
    }
    text_setNormalTextColor(255, 255, 255);
    sTextN = 0;
}

void render_hud(Gfx **gfx, Mtx **mtx, Vtx **vtx) {
    s32 i;

    if (nbMode == MODE_GARAGE) {
        hud_rect(6, 6, SCREEN_W - 12, 34, 10, 10, 30, 150);
        hud_rect(6, 172, SCREEN_W - 12, 34, 10, 10, 30, 150);
        if (nbGarage.menu) hud_rect(52, 46, 188, 120, 10, 10, 40, 200);
    }
    if (sRectN == 0) return;

    gDPPipeSync((*gfx)++);
    gDPSetCycleType((*gfx)++, G_CYC_1CYCLE);
    gDPSetRenderMode((*gfx)++, G_RM_XLU_SURF, G_RM_XLU_SURF2);
    gDPSetCombineMode((*gfx)++, G_CC_PRIMITIVE, G_CC_PRIMITIVE);
    for (i = 0; i < sRectN; i++) {
        Rect *r = &sRects[i];
        gDPPipeSync((*gfx)++);
        gDPSetPrimColor((*gfx)++, 0, 0, r->r, r->g, r->b, r->a);
        gDPFillRectangle((*gfx)++, r->x, r->y, r->x + r->w, r->y + r->h);
    }
    gDPPipeSync((*gfx)++);
    gDPSetCycleType((*gfx)++, G_CYC_2CYCLE);
    sRectN = 0;
}
