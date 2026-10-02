#include "nb.h"

const char *nbCategoryNames[CAT_COUNT] = {
    "BODY", "WHEELS", "POWER", "FLIGHT", "GADGETS", "DECOR"
};

/* colour 0 = natural (no tint) */
const u8 nbPalette[NB_NUM_COLORS][3] = {
    {255, 255, 255}, {235, 60, 50}, {250, 150, 40}, {250, 220, 60}, {90, 200, 70},
    {70, 120, 235},  {160, 80, 210}, {250, 130, 190}, {140, 140, 150}, {60, 60, 70},
};
const char *nbColorNames[NB_NUM_COLORS] = {
    "NATURAL", "RED", "ORANGE", "YELLOW", "GREEN", "BLUE", "PURPLE", "PINK", "GREY", "BLACK"
};

/*  name              cat         kind          sx sy sz flags                unlock col  mass   hp   value */
const NBPartDef nbPartDefs[] = {
    {"WOOD BLOCK",      CAT_BODY,   KIND_BLOCK,   1, 1, 1, PF_PAINT,            0,  0,  2.0f,  30,  0},
    {"METAL BLOCK",     CAT_BODY,   KIND_BLOCK,   1, 1, 1, PF_PAINT,            0,  0,  5.0f,  80,  0},
    {"WOOD WEDGE",      CAT_BODY,   KIND_BLOCK,   1, 1, 1, PF_PAINT,            0,  0,  1.5f,  25,  0},
    {"METAL WEDGE",     CAT_BODY,   KIND_BLOCK,   1, 1, 1, PF_PAINT,            1,  0,  3.5f,  60,  0},
    {"WOOD SLAB",       CAT_BODY,   KIND_BLOCK,   1, 1, 1, PF_PAINT,            0,  0,  1.0f,  20,  0},
    {"LONG PLANK",      CAT_BODY,   KIND_BLOCK,   1, 1, 3, PF_PAINT,            0,  0,  3.0f,  40,  0},
    {"METAL GIRDER",    CAT_BODY,   KIND_BLOCK,   1, 1, 3, PF_PAINT,            2,  0,  4.0f,  90,  0},
    {"GLASS BLOCK",     CAT_BODY,   KIND_BLOCK,   1, 1, 1, 0,                   1,  0,  1.0f,  10,  0},
    {"HONEYCOMB BLOCK", CAT_BODY,   KIND_BLOCK,   1, 1, 1, 0,                   1,  0,  1.0f,  25,  0},
    {"JIGGY BLOCK",     CAT_BODY,   KIND_BLOCK,   1, 1, 1, 0,                   5,  0,  3.0f,  60,  0},
    {"RUBBER BUMPER",   CAT_BODY,   KIND_BLOCK,   1, 1, 1, 0,                   2,  0,  2.0f, 150,  0},

    {"SMALL WHEEL",     CAT_MOVE,   KIND_WHEEL,   1, 1, 1, PF_PAINT | PF_SPIN_X, 0,  0,  1.5f,  40, 30.0f},
    {"BIG WHEEL",       CAT_MOVE,   KIND_WHEEL,   1, 2, 2, PF_PAINT | PF_SPIN_X, 3,  0,  4.0f,  70, 45.0f},
    {"MONSTER WHEEL",   CAT_MOVE,   KIND_WHEEL,   1, 3, 3, PF_PAINT | PF_SPIN_X, 10, 0,  8.0f, 120, 64.0f},
    {"SKI",             CAT_MOVE,   KIND_SKI,     1, 1, 2, PF_PAINT,            2,  0,  1.0f,  30, 20.0f},
    {"TANK TREAD",      CAT_MOVE,   KIND_TREAD,   1, 1, 3, 0,                   8,  0,  6.0f, 120, 22.0f},

    {"SMALL ENGINE",    CAT_POWER,  KIND_ENGINE,  1, 1, 1, 0,                   0,  0,  4.0f,  50, 1.0f},
    {"BIG ENGINE",      CAT_POWER,  KIND_ENGINE,  2, 1, 2, 0,                   4,  0, 10.0f, 100, 3.0f},
    {"FUEL CAN",        CAT_POWER,  KIND_FUEL,    1, 1, 1, PF_PAINT,            0,  1,  1.5f,  20, 100.0f},
    {"FUEL TANK",       CAT_POWER,  KIND_FUEL,    1, 1, 2, PF_PAINT,            2,  1,  3.0f,  40, 250.0f},
    {"SEAT",            CAT_POWER,  KIND_SEAT,    1, 1, 1, PF_PAINT,            0,  5,  1.5f,  40,  0},

    {"PROPELLER",       CAT_FLIGHT, KIND_PROP,    1, 1, 1, PF_SPIN_Z,           3,  0,  2.0f,  30, 1.0f},
    {"JET BOOSTER",     CAT_FLIGHT, KIND_JET,     1, 1, 2, 0,                   8,  0,  3.0f,  40, 3.0f},
    {"WING",            CAT_FLIGHT, KIND_WING,    3, 1, 2, PF_PAINT,            5,  6,  3.0f,  40, 6.0f},
    {"TAIL FIN",        CAT_FLIGHT, KIND_FIN,     1, 1, 1, PF_PAINT,            5,  1,  0.5f,  20, 1.0f},
    {"FLOATER",         CAT_FLIGHT, KIND_FLOATER, 1, 1, 1, 0,                   2,  0,  1.0f,  30, 1.0f},
    {"BALLOON",         CAT_FLIGHT, KIND_BALLOON, 1, 2, 1, PF_PAINT,            6,  1,  0.3f,  10, 1.0f},

    {"SHOCK SPRING",    CAT_GADGET, KIND_SPRING,  1, 1, 1, 0,                   2,  0,  2.0f,  50,  0},
    {"EGG CANNON",      CAT_GADGET, KIND_CANNON,  1, 1, 2, PF_PAINT,            4,  4,  3.0f,  50,  0},
    {"HORN",            CAT_GADGET, KIND_HORN,    1, 1, 1, 0,                   0,  0,  0.5f,  20,  0},
    {"HEADLIGHT",       CAT_GADGET, KIND_LIGHT,   1, 1, 1, 0,                   0,  0,  0.5f,  10,  0},

    {"MUMBO SKULL",     CAT_DECOR,  KIND_DECOR,   1, 1, 1, 0,                   1,  0,  0.5f,  20,  0},
    {"MUSIC NOTE",      CAT_DECOR,  KIND_DECOR,   1, 1, 1, 0,                   0,  0,  0.2f,  10,  0},
    {"CHECKER FLAG",    CAT_DECOR,  KIND_DECOR,   1, 2, 1, 0,                   0,  0,  0.3f,  10,  0},
    {"EXHAUST PIPE",    CAT_DECOR,  KIND_DECOR,   1, 1, 1, 0,                   0,  0,  0.5f,  20,  0},
    {"FRONT GRILLE",    CAT_DECOR,  KIND_DECOR,   1, 1, 1, PF_PAINT,            0,  0,  1.0f,  30,  0},
};
const s32 nbPartCount = sizeof(nbPartDefs) / sizeof(nbPartDefs[0]);

/* ------------------------------------------------------------------ */

u32 nbBlueprintRev;

void bp_clear(NBBlueprint *bp) {
    bp->count = 0;
    nbBlueprintRev++;
}

void bp_partExtent(const NBPart *p, s32 *sx, s32 *sy, s32 *sz) {
    const NBPartDef *d = &nbPartDefs[p->type];
    if (p->rot & 1) {
        *sx = d->sz; *sz = d->sx;
    } else {
        *sx = d->sx; *sz = d->sz;
    }
    *sy = d->sy;
}

static s32 inside(const NBPart *p, s32 x, s32 y, s32 z) {
    s32 sx, sy, sz;
    bp_partExtent(p, &sx, &sy, &sz);
    return x >= p->x && x < p->x + sx && y >= p->y && y < p->y + sy && z >= p->z && z < p->z + sz;
}

s32 bp_findAt(const NBBlueprint *bp, s32 x, s32 y, s32 z) {
    s32 i;
    for (i = 0; i < bp->count; i++) {
        if (inside(&bp->parts[i], x, y, z)) return i;
    }
    return -1;
}

/* Returns 1 if placeable, 0 = blocked/out of bounds, -1 = not attached,
 * -2 = blueprint full. */
s32 bp_canPlace(const NBBlueprint *bp, s32 type, s32 x, s32 y, s32 z, s32 rot) {
    NBPart tmp;
    s32 sx, sy, sz, i, j, k, attached = 0;

    if (bp->count >= MAX_PARTS) return -2;
    tmp.type = type; tmp.rot = rot; tmp.x = x; tmp.y = y; tmp.z = z;
    bp_partExtent(&tmp, &sx, &sy, &sz);
    if (x < -GRID_HALF || x + sx - 1 > GRID_HALF) return 0;
    if (z < -GRID_HALF || z + sz - 1 > GRID_HALF) return 0;
    if (y < 0 || y + sy > GRID_H) return 0;

    for (i = x; i < x + sx; i++)
        for (j = y; j < y + sy; j++)
            for (k = z; k < z + sz; k++)
                if (bp_findAt(bp, i, j, k) >= 0) return 0;

    if (bp->count == 0) return 1;
    /* must share a face with an existing part */
    for (i = x - 1; i <= x + sx && !attached; i++)
        for (j = y - 1; j <= y + sy && !attached; j++)
            for (k = z - 1; k <= z + sz && !attached; k++) {
                s32 out = (i < x || i >= x + sx) + (j < y || j >= y + sy) + (k < z || k >= z + sz);
                if (out == 1 && bp_findAt(bp, i, j, k) >= 0) attached = 1;
            }
    return attached ? 1 : -1;
}

s32 bp_add(NBBlueprint *bp, s32 type, s32 x, s32 y, s32 z, s32 rot, s32 color) {
    NBPart *p;
    if (bp_canPlace(bp, type, x, y, z, rot) != 1) return -1;
    p = &bp->parts[bp->count];
    p->type = type; p->x = x; p->y = y; p->z = z; p->rot = rot; p->color = color;
    nbBlueprintRev++;
    return bp->count++;
}

void bp_remove(NBBlueprint *bp, s32 index) {
    s32 i;
    if (index < 0 || index >= bp->count) return;
    for (i = index; i < bp->count - 1; i++) bp->parts[i] = bp->parts[i + 1];
    bp->count--;
    nbBlueprintRev++;
}

/* centre of the occupied cells in blueprint units (cell x is centred at x*CELL,
 * cell y spans [y*CELL, (y+1)*CELL]) */
void bp_partCenter(const NBPart *p, f32 out[3]) {
    s32 sx, sy, sz;
    bp_partExtent(p, &sx, &sy, &sz);
    out[0] = (p->x + (sx - 1) * 0.5f) * CELL;
    out[1] = (p->y + sy * 0.5f) * CELL;
    out[2] = (p->z + (sz - 1) * 0.5f) * CELL;
}

s32 bp_countKind(const NBBlueprint *bp, s32 kind) {
    s32 i, n = 0;
    for (i = 0; i < bp->count; i++)
        if (nbPartDefs[bp->parts[i].type].kind == kind) n++;
    return n;
}

/* ------------------------------------------------------------------ */
/* Preset vehicles                                                     */

enum {
    P_WOOD, P_METAL, P_WWEDGE, P_MWEDGE, P_SLAB, P_PLANK, P_GIRDER, P_GLASS, P_HONEY, P_JIGGY, P_BUMPER,
    P_WHEEL, P_BIGWHEEL, P_MONSTER, P_SKI, P_TREAD,
    P_ENGINE, P_BIGENGINE, P_FUELCAN, P_FUELTANK, P_SEAT,
    P_PROP, P_JET, P_WING, P_FIN, P_FLOATER, P_BALLOON,
    P_SPRING, P_CANNON, P_HORN, P_LIGHT,
    P_SKULL, P_NOTE, P_FLAG, P_EXHAUST, P_GRILLE
};

const char *nbPresetNames[NB_NUM_PRESETS] = {
    "TROLLEY", "KAZOOIE KART", "GLIDER", "BOAT", "BALLOON BUS", "MONSTER TRUCK"
};

static void box(NBBlueprint *bp, s32 type, s32 x0, s32 x1, s32 y, s32 z0, s32 z1, s32 color) {
    s32 x, z;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++)
            bp_add(bp, type, x, y, z, 0, color);
}

void preset_build(s32 index, NBBlueprint *bp) {
    bp_clear(bp);
    switch (index) {
    case 0: /* TROLLEY: the classic starter cart */
        box(bp, P_WOOD, -1, 1, 0, -1, 2, 0);
        bp_add(bp, P_SEAT, 0, 1, 0, 0, 5);
        bp_add(bp, P_ENGINE, 0, 1, -1, 0, 0);
        bp_add(bp, P_FUELCAN, 1, 1, -1, 0, 1);
        bp_add(bp, P_EXHAUST, -1, 1, -1, 0, 0);
        bp_add(bp, P_GRILLE, 0, 1, 2, 0, 1);
        bp_add(bp, P_LIGHT, 1, 1, 2, 0, 0);
        bp_add(bp, P_LIGHT, -1, 1, 2, 0, 0);
        bp_add(bp, P_HORN, 1, 1, 1, 0, 0);
        bp_add(bp, P_WHEEL, 2, 0, -1, 0, 0);
        bp_add(bp, P_WHEEL, 2, 0, 2, 0, 0);
        bp_add(bp, P_WHEEL, -2, 0, -1, 0, 0);
        bp_add(bp, P_WHEEL, -2, 0, 2, 0, 0);
        break;
    case 1: /* KAZOOIE KART: low metal racer */
        box(bp, P_WOOD, -1, 1, 0, -2, 1, 1);
        bp_add(bp, P_WWEDGE, -1, 0, 2, 0, 1);
        bp_add(bp, P_MWEDGE, 0, 0, 2, 0, 3);
        bp_add(bp, P_WWEDGE, 1, 0, 2, 0, 1);
        bp_add(bp, P_SEAT, 0, 1, 0, 0, 8);
        bp_add(bp, P_ENGINE, -1, 1, -2, 0, 0);
        bp_add(bp, P_ENGINE, 1, 1, -2, 0, 0);
        bp_add(bp, P_FUELTANK, 0, 1, -2, 0, 3);
        bp_add(bp, P_FLAG, 1, 1, -1, 0, 0);
        bp_add(bp, P_WHEEL, 2, 0, -2, 0, 9);
        bp_add(bp, P_WHEEL, 2, 0, 1, 0, 9);
        bp_add(bp, P_WHEEL, -2, 0, -2, 0, 9);
        bp_add(bp, P_WHEEL, -2, 0, 1, 0, 9);
        bp_add(bp, P_CANNON, 0, 1, 1, 0, 1);
        break;
    case 2: /* GLIDER */
        box(bp, P_WOOD, 0, 0, 1, -3, 2, 3);
        bp_add(bp, P_SEAT, 0, 2, 0, 0, 5);
        bp_add(bp, P_FUELCAN, 0, 2, -1, 0, 1);
        bp_add(bp, P_FIN, 0, 2, -3, 0, 1);
        bp_add(bp, P_WING, 1, 1, -1, 0, 6);
        bp_add(bp, P_WING, -3, 1, -1, 0, 6);
        bp_add(bp, P_PROP, 0, 1, 3, 0, 0);
        bp_add(bp, P_ENGINE, 0, 2, -2, 0, 0);
        bp_add(bp, P_WHEEL, 1, 0, -1, 0, 0);
        bp_add(bp, P_WHEEL, -1, 0, -1, 0, 0);
        bp_add(bp, P_WHEEL, 0, 0, 2, 0, 0);
        break;
    case 3: /* BOAT */
        box(bp, P_WOOD, -1, 1, 0, -2, 1, 0);
        bp_add(bp, P_WWEDGE, -1, 0, 2, 0, 0);
        bp_add(bp, P_WWEDGE, 0, 0, 2, 0, 0);
        bp_add(bp, P_WWEDGE, 1, 0, 2, 0, 0);
        box(bp, P_FLOATER, 2, 2, 0, -2, 1, 0);
        box(bp, P_FLOATER, -2, -2, 0, -2, 1, 0);
        bp_add(bp, P_SEAT, 0, 1, -1, 0, 5);
        bp_add(bp, P_FUELCAN, 1, 1, -2, 0, 1);
        bp_add(bp, P_FIN, 0, 1, -2, 0, 6);
        bp_add(bp, P_PROP, 0, 0, -3, 0, 0);
        bp_add(bp, P_HORN, 1, 1, 0, 0, 0);
        break;
    case 4: /* BALLOON BUS */
        box(bp, P_WOOD, -1, 1, 0, -1, 1, 3);
        bp_add(bp, P_SEAT, 0, 1, 0, 0, 5);
        bp_add(bp, P_BALLOON, 1, 1, 1, 0, 1);
        bp_add(bp, P_BALLOON, -1, 1, 1, 0, 3);
        bp_add(bp, P_BALLOON, 1, 1, -1, 0, 4);
        bp_add(bp, P_BALLOON, -1, 1, -1, 0, 5);
        bp_add(bp, P_FUELCAN, 0, 1, -1, 0, 1);
        bp_add(bp, P_PROP, 0, 0, -2, 0, 0);
        bp_add(bp, P_FIN, 0, 1, 1, 0, 6);
        break;
    case 5: /* MONSTER TRUCK */
        box(bp, P_METAL, -1, 1, 1, -2, 2, 5);
        bp_add(bp, P_BIGWHEEL, 2, 0, -2, 0, 9);
        bp_add(bp, P_BIGWHEEL, 2, 0, 1, 0, 9);
        bp_add(bp, P_BIGWHEEL, -2, 0, -2, 0, 9);
        bp_add(bp, P_BIGWHEEL, -2, 0, 1, 0, 9);
        bp_add(bp, P_SEAT, 0, 2, 0, 0, 1);
        bp_add(bp, P_BIGENGINE, -1, 2, -2, 0, 0);
        bp_add(bp, P_FUELTANK, 1, 2, -2, 0, 3);
        bp_add(bp, P_JET, -1, 2, 1, 0, 0);
        bp_add(bp, P_JET, 1, 2, 1, 0, 0);
        bp_add(bp, P_SKULL, 0, 2, 2, 0, 0);
        bp_add(bp, P_SPRING, 0, 2, 1, 0, 0);
        bp_add(bp, P_HORN, 1, 2, 0, 0, 0);
        break;
    }
}
