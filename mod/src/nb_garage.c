/*
 * Mumbo's Garage: the in-world build mode.
 *
 * A 15x8x15 grid is laid out on the ground where Banjo stood. Banjo is hidden
 * and frozen while you build; the camera orbits the grid.
 */
#include "nb.h"

NBGarage nbGarage;
extern s32 nbSandbox;

enum {
    MENU_DRIVE, MENU_LOAD, MENU_SAVE, MENU_SANDBOX, MENU_CLEAR, MENU_LEAVE, MENU_BACK, MENU_COUNT
};

static f32 sCamYawTarget;
static f32 sEye[3], sTarget[3];
static s32 sCamInit;
static f32 sHintClock;
static char sLine[8][40];

void garage_cellToWorld(f32 cx, f32 cy, f32 cz, f32 out[3]) {
    f32 l[3];
    v3_set(l, cx * CELL, cy * CELL, cz * CELL);
    v3_rotateYaw(out, l, nbGarage.yaw);
    v3_add(out, out, nbGarage.origin);
}

static void localToWorld(const f32 l[3], f32 out[3]) {
    v3_rotateYaw(out, l, nbGarage.yaw);
    v3_add(out, out, nbGarage.origin);
}

void garage_enter(const f32 floorPos[3], f32 yawDeg) {
    NBGarage *g = &nbGarage;
    s32 i, top = -1;

    g->active = TRUE;
    v3_copy(g->origin, floorPos);
    g->yaw = yawDeg;
    for (i = 0; i < nbBlueprint.count; i++) {
        s32 sx, sy, sz;
        bp_partExtent(&nbBlueprint.parts[i], &sx, &sy, &sz);
        if (nbBlueprint.parts[i].y + sy - 1 > top) top = nbBlueprint.parts[i].y + sy - 1;
    }
    g->cx = 0;
    g->cz = 0;
    g->cy = (top + 1 < GRID_H) ? top + 1 : GRID_H - 1;
    g->rot = 0;
    g->color = nbPartDefs[g->type].defaultColor;
    g->camYaw = sCamYawTarget = 35.0f;
    g->camDist = 1.0f;
    g->menu = FALSE;
    g->holdB = 0;
    g->repeat = 0;
    g->lastDir = 0;
    sCamInit = FALSE;
    sHintClock = 0;

    nb_memset(nbVeh.broken, 0, sizeof(nbVeh.broken));
    nb_setMode(MODE_GARAGE);
    gcsfx_playWithPitch(SFX_30_MAGIC_POOF, 1.0f, 26000);
    nb_message("WELCOME TO MUMBO'S GARAGE!", 2.0f);
}

/* stats of the current blueprint for the HUD */
static void blueprintStats(f32 *mass, f32 *power, f32 *fuel, s32 *seats) {
    s32 i;
    *mass = *power = *fuel = 0;
    *seats = 0;
    for (i = 0; i < nbBlueprint.count; i++) {
        const NBPartDef *d = &nbPartDefs[nbBlueprint.parts[i].type];
        *mass += d->mass;
        if (d->kind == KIND_ENGINE) *power += d->value;
        if (d->kind == KIND_FUEL) *fuel += d->value;
        if (d->kind == KIND_SEAT) (*seats)++;
    }
}

static void finishGarage(s32 drive) {
    NBGarage *g = &nbGarage;
    s32 n;

    nb_memset(nbVeh.broken, 0, sizeof(nbVeh.broken));
    n = vehicle_compile(&nbVeh, &nbBlueprint);
    if (n < 0) {
        if (drive) {
            nb_message(nbBlueprint.count ? "NEEDS A SEAT!" : "BUILD SOMETHING FIRST!", 2.0f);
            gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 0.7f, 24000);
            return;
        }
        /* leave without a vehicle */
        nb_setMode(MODE_NORMAL);
        func_8028F85C(g->origin);
        return;
    }
    {
        s32 i;
        for (i = 0; i < nbBlueprint.count; i++) nbVeh.hp[i] = nbPartDefs[nbBlueprint.parts[i].type].hp;
    }
    vehicle_spawn(&nbVeh, g->origin, g->yaw);
    g->active = FALSE;
    if (n < nbBlueprint.count) {
        char buf[40], num[8];
        nb_itoa(num, nbBlueprint.count - n);
        nb_strcpy(buf, num);
        nb_strcat(buf, " LOOSE PARTS LEFT OFF");
        nb_message(buf, 2.5f);
    }
    if (drive) {
        nb_setMode(MODE_DRIVE);
        gcsfx_playWithPitch(SFX_C_TAKING_FLIGHT_LIFTOFF, 1.0f, 26000);
        if (nbVeh.fuelMax <= 0 && (nbVeh.enginePower > 0 || nbVeh.numThrust > 0))
            nb_message("NO FUEL! ADD A FUEL CAN", 2.5f);
    } else {
        nb_exitVehicle();
    }
}

static void cycleType(s32 dir) {
    NBGarage *g = &nbGarage;
    g->type = (g->type + dir + nbPartCount) % nbPartCount;
    g->color = nbPartDefs[g->type].defaultColor;
    gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.2f, 18000);
}

static void cycleCategory(s32 dir) {
    NBGarage *g = &nbGarage;
    s32 cat = (nbPartDefs[g->type].category + dir + CAT_COUNT) % CAT_COUNT, i;
    for (i = 0; i < nbPartCount; i++) {
        if (nbPartDefs[i].category == cat) {
            g->type = i;
            break;
        }
    }
    g->color = nbPartDefs[g->type].defaultColor;
    gcsfx_playWithPitch(SFX_CC_PAUSEMENU_ENTER_SUBMENU, 1.0f, 20000);
}

static void moveCursor(f32 dt) {
    NBGarage *g = &nbGarage;
    f32 mag = nb_sqrtf(nbIn.sx * nbIn.sx + nbIn.sy * nbIn.sy);
    s32 dx = 0, dz = 0, dir;

    if (mag < 0.45f) {
        g->repeat = 0;
        g->lastDir = 0;
        return;
    }
    {
        f32 a = DEG2RAD(g->camYaw);
        f32 f[2] = {sinf(a), cosf(a)};      /* camera look dir in grid space (x, z) */
        f32 r[2] = {-cosf(a), sinf(a)};     /* camera right */
        f32 mx = f[0] * nbIn.sy + r[0] * nbIn.sx;
        f32 mz = f[1] * nbIn.sy + r[1] * nbIn.sx;
        if (nb_absf(mx) > nb_absf(mz)) dx = mx > 0 ? 1 : -1;
        else dz = mz > 0 ? 1 : -1;
    }
    dir = dx * 3 + dz;
    if (dir != g->lastDir) {
        g->lastDir = dir;
        g->repeat = 0.32f;
    } else {
        g->repeat -= dt;
        if (g->repeat > 0) return;
        g->repeat = 0.11f;
    }
    g->cx = (s32)nb_clampf(g->cx + dx, -GRID_HALF, GRID_HALF);
    g->cz = (s32)nb_clampf(g->cz + dz, -GRID_HALF, GRID_HALF);
    gcsfx_playWithPitch(SFX_90_SWITCH_PRESS, 1.6f, 9000);
}

static void tryPlace(void) {
    NBGarage *g = &nbGarage;
    s32 r;
    if (!nb_partUnlocked(g->type)) {
        nb_message("COLLECT MORE JIGGIES!", 1.5f);
        gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 0.7f, 22000);
        return;
    }
    r = bp_canPlace(&nbBlueprint, g->type, g->cx, g->cy, g->cz, g->rot);
    if (r == 1) {
        bp_add(&nbBlueprint, g->type, g->cx, g->cy, g->cz, g->rot, g->color);
        gcsfx_playWithPitch(SFX_20_METAL_CLANK_1, 0.9f + (g->cy % 3) * 0.1f, 26000);
    } else {
        nb_message(r == 0 ? "NO ROOM THERE!" : r == -1 ? "ATTACH IT TO YOUR VEHICLE!" : "TOO MANY PARTS!", 1.5f);
        gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 0.7f, 22000);
    }
}

/* a preset can be loaded once every part in it is unlocked */
static s32 presetLocked(s32 preset) {
    static NBBlueprint tmp;
    s32 i;
    preset_build(preset, &tmp);
    for (i = 0; i < tmp.count; i++)
        if (!nb_partUnlocked(tmp.parts[i].type)) return TRUE;
    return FALSE;
}

static const char *loadName(s32 i) {
    static char buf[16];
    if (i < NUM_SLOTS) {
        nb_strcpy(buf, "SLOT ");
        buf[5] = '1' + i;
        buf[6] = 0;
        return buf;
    }
    return nbPresetNames[i - NUM_SLOTS];
}

static void menuUpdate(void) {
    NBGarage *g = &nbGarage;
    s32 lr = 0;

    if ((nbIn.pressed & BTN_DD) || (nbIn.sy < -0.6f && g->repeat <= 0)) { g->menuSel = (g->menuSel + 1) % MENU_COUNT; g->repeat = 0.2f; gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.2f, 16000); }
    if ((nbIn.pressed & BTN_DU) || (nbIn.sy > 0.6f && g->repeat <= 0)) { g->menuSel = (g->menuSel + MENU_COUNT - 1) % MENU_COUNT; g->repeat = 0.2f; gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.2f, 16000); }
    if ((nbIn.pressed & (BTN_DR | BTN_R)) || (nbIn.sx > 0.6f && g->repeat <= 0)) { lr = 1; g->repeat = 0.2f; }
    if ((nbIn.pressed & (BTN_DL | BTN_L)) || (nbIn.sx < -0.6f && g->repeat <= 0)) { lr = -1; g->repeat = 0.2f; }
    if (nb_absf(nbIn.sx) < 0.3f && nb_absf(nbIn.sy) < 0.3f) g->repeat = 0;

    if (lr) {
        if (g->menuSel == MENU_LOAD) g->menuBlueprint = (g->menuBlueprint + lr + NUM_SLOTS + NB_NUM_PRESETS) % (NUM_SLOTS + NB_NUM_PRESETS);
        if (g->menuSel == MENU_SAVE) g->menuSaveSlot = (g->menuSaveSlot + lr + NUM_SLOTS) % NUM_SLOTS;
        gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.4f, 16000);
    }

    if (nbIn.pressed & (BTN_B | BTN_START)) {
        g->menu = FALSE;
        gcsfx_playWithPitch(SFX_CD_PAUSEMENU_LEAVE_SUBMENU, 1.0f, 22000);
        return;
    }
    if (!(nbIn.pressed & BTN_A)) return;

    switch (g->menuSel) {
    case MENU_DRIVE:
        g->menu = FALSE;
        finishGarage(TRUE);
        break;
    case MENU_LOAD:
        if (g->menuBlueprint < NUM_SLOTS) {
            if (nbSlots[g->menuBlueprint].count == 0) {
                nb_message("THAT SLOT IS EMPTY", 1.5f);
                break;
            }
            nbBlueprint = nbSlots[g->menuBlueprint];
            nbBlueprintRev++;
        } else {
            if (presetLocked(g->menuBlueprint - NUM_SLOTS)) {
                nb_message("COLLECT MORE JIGGIES FIRST!", 1.8f);
                gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 0.7f, 22000);
                break;
            }
            preset_build(g->menuBlueprint - NUM_SLOTS, &nbBlueprint);
        }
        nb_message("BLUEPRINT LOADED!", 1.5f);
        gcsfx_playWithPitch(SFX_30_MAGIC_POOF, 1.0f, 24000);
        g->menu = FALSE;
        break;
    case MENU_SAVE:
        nbSlots[g->menuSaveSlot] = nbBlueprint;
        nb_message("SAVED UNTIL POWER OFF", 1.8f);
        gcsfx_playWithPitch(SFX_C9_PAUSEMENU_ENTER, 1.0f, 24000);
        g->menu = FALSE;
        break;
    case MENU_SANDBOX:
        nbSandbox = !nbSandbox;
        gcsfx_playWithPitch(SFX_C9_PAUSEMENU_ENTER, 1.2f, 22000);
        break;
    case MENU_CLEAR:
        bp_clear(&nbBlueprint);
        g->cx = g->cz = 0;
        g->cy = 0;
        nb_message("ALL PARTS REMOVED", 1.5f);
        gcsfx_playWithPitch(SFX_D9_WOODEN_CRATE_BREAKING_1, 1.0f, 26000);
        g->menu = FALSE;
        break;
    case MENU_LEAVE:
        g->menu = FALSE;
        finishGarage(FALSE);
        break;
    case MENU_BACK:
        g->menu = FALSE;
        break;
    }
}

static void hud(void) {
    NBGarage *g = &nbGarage;
    const NBPartDef *d = &nbPartDefs[g->type];
    f32 mass, power, fuel;
    s32 seats;
    char num[12];

    blueprintStats(&mass, &power, &fuel, &seats);

    if (g->menu) {
        static const char *items[MENU_COUNT] = {"TEST DRIVE!", "LOAD ", "SAVE TO ", "", "CLEAR ALL PARTS", "LEAVE GARAGE", "BACK"};
        s32 i;
        hud_queueText(100, 56, "GARAGE MENU", 255, 220, 80);
        for (i = 0; i < MENU_COUNT; i++) {
            char *l = sLine[i];
            nb_strcpy(l, i == g->menuSel ? "- " : "  ");
            nb_strcat(l, items[i]);
            if (i == MENU_LOAD) {
                nb_strcat(l, loadName(g->menuBlueprint));
                if (g->menuBlueprint >= NUM_SLOTS && presetLocked(g->menuBlueprint - NUM_SLOTS)) nb_strcat(l, " (LOCKED)");
            }
            if (i == MENU_SAVE) nb_strcat(l, loadName(g->menuSaveSlot));
            if (i == MENU_SANDBOX) nb_strcat(l, nbSandbox ? "PARTS: ALL (SANDBOX)" : "PARTS: BY JIGGIES");
            if (i == g->menuSel) hud_queueText(62, 76 + i * 14, l, 255, 255, 255);
            else hud_queueText(62, 76 + i * 14, l, 170, 170, 190);
        }
        return;
    }

    /* top: selected part */
    nb_strcpy(sLine[0], nbCategoryNames[d->category]);
    nb_strcat(sLine[0], ": ");
    nb_strcat(sLine[0], d->name);
    hud_queueText(16, 16, sLine[0], 255, 220, 80);

    if (!nb_partUnlocked(g->type)) {
        nb_strcpy(sLine[1], "LOCKED - NEEDS ");
        nb_itoa(num, d->unlock);
        nb_strcat(sLine[1], num);
        nb_strcat(sLine[1], " JIGGIES (HAVE ");
        nb_itoa(num, item_getCount(ITEM_26_JIGGY_TOTAL));
        nb_strcat(sLine[1], num);
        nb_strcat(sLine[1], ")");
        hud_queueText(16, 30, sLine[1], 255, 90, 70);
    } else {
        nb_strcpy(sLine[1], "WT ");
        nb_itoa(num, (s32)(d->mass + 0.5f));
        nb_strcat(sLine[1], num);
        nb_strcat(sLine[1], "  HP ");
        nb_itoa(num, d->hp);
        nb_strcat(sLine[1], num);
        if (d->flags & PF_PAINT) {
            nb_strcat(sLine[1], "  ");
            nb_strcat(sLine[1], nbColorNames[g->color]);
        }
        hud_queueText(16, 30, sLine[1], 220, 220, 230);
    }

    /* bottom: vehicle stats + rotating hints */
    nb_strcpy(sLine[2], "PARTS ");
    nb_itoa(num, nbBlueprint.count);
    nb_strcat(sLine[2], num);
    nb_strcat(sLine[2], "  WT ");
    nb_itoa(num, (s32)mass);
    nb_strcat(sLine[2], num);
    nb_strcat(sLine[2], "  PWR ");
    nb_itoa(num, (s32)power);
    nb_strcat(sLine[2], num);
    nb_strcat(sLine[2], "  FUEL ");
    nb_itoa(num, (s32)fuel);
    nb_strcat(sLine[2], num);
    hud_queueText(16, 182, sLine[2], seats ? 200 : 255, seats ? 230 : 120, seats ? 255 : 100);

    {
        /* note: the dialog font draws / < > ^ as controller button icons */
        static const char *hints[6] = {
            "A PLACE   B REMOVE   Z ROTATE",
            "L, R: CHANGE PART",
            "D-PAD: PART TYPE AND COLOR",
            "C-UP, C-DOWN: CHANGE LAYER",
            "C-LEFT, C-RIGHT: TURN CAMERA",
            "START: MENU AND TEST DRIVE",
        };
        s32 hi = ((s32)(sHintClock / 2.5f)) % 6;
        if (seats == 0 && nbBlueprint.count > 0 && hi == 5) hud_queueText(16, 196, "ADD A SEAT FOR BANJO!", 255, 140, 90);
        else hud_queueText(16, 196, hints[hi], 170, 200, 255);
    }
    if (g->holdB > 0.3f) hud_queueText(62, 130, "KEEP HOLDING B: CLEAR", 255, 100, 80);
}

void garage_update(f32 dt) {
    NBGarage *g = &nbGarage;

    sHintClock += dt;
    g->blink += dt;
    if (g->repeat > 0 && g->menu) g->repeat -= dt;

    if (g->menu) {
        menuUpdate();
        if (nbMode == MODE_GARAGE) hud();
        return;
    }

    if (nbIn.pressed & BTN_START) {
        g->menu = TRUE;
        g->menuSel = MENU_DRIVE;
        g->repeat = 0.2f;
        gcsfx_playWithPitch(SFX_CC_PAUSEMENU_ENTER_SUBMENU, 1.0f, 22000);
        hud();
        return;
    }

    moveCursor(dt);
    if (nbIn.pressed & BTN_CU) { if (g->cy < GRID_H - 1) g->cy++; gcsfx_playWithPitch(SFX_90_SWITCH_PRESS, 1.8f, 9000); }
    if (nbIn.pressed & BTN_CD) { if (g->cy > 0) g->cy--; gcsfx_playWithPitch(SFX_90_SWITCH_PRESS, 1.4f, 9000); }
    if (nbIn.pressed & BTN_CL) sCamYawTarget -= 45.0f;
    if (nbIn.pressed & BTN_CR) sCamYawTarget += 45.0f;
    if (nbIn.pressed & BTN_R) cycleType(1);
    if (nbIn.pressed & BTN_L) cycleType(-1);
    if (nbIn.pressed & BTN_DR) cycleCategory(1);
    if (nbIn.pressed & BTN_DL) cycleCategory(-1);
    if (nbIn.pressed & BTN_DU) { g->color = (g->color + 1) % NB_NUM_COLORS; gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.5f, 16000); }
    if (nbIn.pressed & BTN_DD) { g->color = (g->color + NB_NUM_COLORS - 1) % NB_NUM_COLORS; gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 1.3f, 16000); }
    if (nbIn.pressed & BTN_Z) { g->rot = (g->rot + 1) & 3; gcsfx_playWithPitch(SFX_A7_WOODEN_SWOSH, 1.2f, 20000); }
    if (nbIn.pressed & BTN_A) tryPlace();

    if (nbIn.pressed & BTN_B) {
        s32 idx = bp_findAt(&nbBlueprint, g->cx, g->cy, g->cz);
        if (idx >= 0) {
            bp_remove(&nbBlueprint, idx);
            gcsfx_playWithPitch(SFX_D9_WOODEN_CRATE_BREAKING_1, 1.2f, 22000);
        }
    }
    if (nbIn.held & BTN_B) {
        g->holdB += dt;
        if (g->holdB > 1.5f) {
            bp_clear(&nbBlueprint);
            g->holdB = 0;
            g->cy = 0;
            nb_message("ALL PARTS REMOVED", 1.5f);
            gcsfx_playWithPitch(SFX_82_METAL_BREAK, 1.0f, 26000);
        }
    } else {
        g->holdB = 0;
    }

    hud();
}

void garage_camera(f32 dt) {
    NBGarage *g = &nbGarage;
    f32 cur[3], mid[3], tl[3], el[3], eye[3], target[3], off[3], k;
    f32 dist = 520.0f;

    g->camYaw = nb_lerpf(g->camYaw, sCamYawTarget, nb_clampf(dt * 8.0f, 0, 1));

    v3_set(cur, g->cx * CELL, (g->cy + 0.5f) * CELL, g->cz * CELL);
    v3_set(mid, 0, CELL * 1.5f, 0);
    tl[0] = nb_lerpf(mid[0], cur[0], 0.6f);
    tl[1] = nb_lerpf(mid[1], cur[1], 0.6f);
    tl[2] = nb_lerpf(mid[2], cur[2], 0.6f);

    v3_set(off, 0, 0, -dist);
    v3_rotateYaw(off, off, g->camYaw);
    v3_add(el, tl, off);
    el[1] += dist * 0.62f + g->cy * CELL * 0.5f;

    localToWorld(tl, target);
    localToWorld(el, eye);
    {
        f32 t2[3];
        v3_copy(t2, target);
        t2[1] += 40.0f;
        {
            f32 end[3], n[3];
            v3_copy(end, eye);
            if (func_80320B98(t2, end, n, NB_FLOOR_FLAGS) != NULL) {
                f32 d[3];
                v3_sub(d, t2, end);
                v3_addScaled(end, d, 0.1f);
                v3_copy(eye, end);
            }
        }
    }

    if (!sCamInit) {
        v3_copy(sEye, eye);
        v3_copy(sTarget, target);
        sCamInit = TRUE;
    }
    k = nb_clampf(dt * 7.0f, 0, 1);
    sEye[0] = nb_lerpf(sEye[0], eye[0], k);
    sEye[1] = nb_lerpf(sEye[1], eye[1], k);
    sEye[2] = nb_lerpf(sEye[2], eye[2], k);
    sTarget[0] = nb_lerpf(sTarget[0], target[0], k);
    sTarget[1] = nb_lerpf(sTarget[1], target[1], k);
    sTarget[2] = nb_lerpf(sTarget[2], target[2], k);
    cam_lookAt(sEye, sTarget);
}
