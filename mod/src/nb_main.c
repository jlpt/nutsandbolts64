/*
 * Nuts & Bolts 64 - entry points called from the game's hooks, the
 * on-foot / garage / driving mode machine, input and cameras.
 */
#include "nb.h"

/* Must match decomp/include/nbhooks.h */
typedef struct {
    u32 magic;
    u32 version;
    u32 load_size;
    u8 *bss_start;
    u8 *bss_end;
    void (*init)(void);
    void (*update)(void);
    void (*draw_world)(Gfx **gfx, Mtx **mtx, Vtx **vtx);
    void (*draw_hud)(Gfx **gfx, Mtx **mtx, Vtx **vtx);
    s32 (*block_pause)(void);
    void (*map_unload)(void);
    u32 pad[5];
} NBHeader;

extern u8 __bss_start[], __bss_end[], __load_size[];

static void nb_init(void);
static void nb_update(void);
static void nb_drawWorld(Gfx **gfx, Mtx **mtx, Vtx **vtx);
static void nb_drawHud(Gfx **gfx, Mtx **mtx, Vtx **vtx);
static s32 nb_blockPause(void);
static void nb_mapUnload(void);

NBHeader nbHeader __attribute__((section(".nbheader"), used)) = {
    0x4E423634, NB_VERSION, (u32)__load_size, __bss_start, __bss_end,
    nb_init, nb_update, nb_drawWorld, nb_drawHud, nb_blockPause, nb_mapUnload,
    {0, 0, 0, 0, 0},
};

s32 nbMode;
NBInput nbIn;
NBBlueprint nbBlueprint;
/* Not zeroed by the loader, so saved blueprints survive the Reset button
 * (RDRAM keeps its contents until the power goes off). */
NBBlueprint nbSlots[NUM_SLOTS] __attribute__((section(".noinit")));
static u32 sSlotsMagic __attribute__((section(".noinit")));
static u32 sSlotsSum __attribute__((section(".noinit")));

static u32 slotsChecksum(void) {
    const u8 *p = (const u8 *)nbSlots;
    u32 i, sum = 0x1234567;
    for (i = 0; i < sizeof(nbSlots); i++) sum = (sum << 5) + (sum >> 27) + p[i];
    return sum;
}

void nb_slotsChanged(void) {
    sSlotsMagic = 0x534C4F54;
    sSlotsSum = slotsChecksum();
}
f32 nbMessageTimer;
char nbMessage[40];
f32 nbCamPos[3], nbCamRot[3];
s32 nbCamActive;
s32 nbSandbox;

static u16 sPrevButtons;
static s32 sMap = -1;
static f32 sDriveCamYaw, sDriveCamDist = 1.0f;
static s32 sDriveCamInit;
static f32 sHintTimer;
static s32 sLockedState = -1;

/* ------------------------------------------------------------------ */

static void nb_init(void) {
    s32 i;
    mesh_buildAll();
    nbMode = MODE_NORMAL;
    preset_build(0, &nbBlueprint);
    if (sSlotsMagic != 0x534C4F54 || sSlotsSum != slotsChecksum()) {
        for (i = 0; i < NUM_SLOTS; i++) bp_clear(&nbSlots[i]);
        nb_slotsChanged();
    }
    nbVeh.active = FALSE;
#ifdef NB_UNLOCK_ALL
    nbSandbox = TRUE;
#endif
}

void input_update(void) {
    OSContPad *pad = joy_getInputsPrimary();
    u16 b = pad->button;
    s32 x = pad->stick_x, y = pad->stick_y;

    nbIn.held = b;
    nbIn.pressed = b & ~sPrevButtons;
    nbIn.released = ~b & sPrevButtons;
    sPrevButtons = b;

    if (x > -8 && x < 8) x = 0;
    if (y > -8 && y < 8) y = 0;
    nbIn.sx = nb_clampf(x / 72.0f, -1.0f, 1.0f);
    nbIn.sy = nb_clampf(y / 72.0f, -1.0f, 1.0f);
}

void nb_message(const char *msg, f32 seconds) {
    nb_strcpy(nbMessage, msg);
    nbMessageTimer = seconds;
}

s32 nb_partUnlocked(s32 type) {
    if (nbSandbox) return TRUE;
    return item_getCount(ITEM_26_JIGGY_TOTAL) >= nbPartDefs[type].unlock;
}

/* ------------------------------------------------------------------ */
/* camera                                                              */

void cam_lookAt(f32 eye[3], f32 target[3]) {
    f32 d[3];
    v3_sub(d, target, eye);
    nbCamRot[1] = RAD2DEG(nb_atan2f(-d[0], -d[2]));
    nbCamRot[0] = RAD2DEG(nb_atan2f(d[1], nb_sqrtf(d[0] * d[0] + d[2] * d[2])));
    nbCamRot[2] = 0.0f;
    v3_copy(nbCamPos, eye);
    viewport_setPosition_vec3f(nbCamPos);
    viewport_setRotation_vec3f(nbCamRot);
    viewport_update();
}

/* keep the camera out of walls: pull it in front of the first hit */
static void cam_collide(f32 target[3], f32 eye[3]) {
    f32 end[3], n[3];
    v3_copy(end, eye);
    if (func_80320B98(target, end, n, NB_FLOOR_FLAGS) != NULL) {
        f32 d[3];
        v3_sub(d, target, end);
        v3_addScaled(end, d, 0.15f);
        v3_addScaled(end, n, 12.0f);
        v3_copy(eye, end);
    }
}

void cam_driveUpdate(f32 dt) {
    NBVehicle *v = &nbVeh;
    f32 target[3], eye[3], fwd[3], off[3];
    f32 yaw, dist, height, k;

    if (nbIn.held & BTN_CL) sDriveCamYaw += 120.0f * dt;
    if (nbIn.held & BTN_CR) sDriveCamYaw -= 120.0f * dt;
    if (nbIn.pressed & BTN_CD) sDriveCamDist = (sDriveCamDist >= 1.6f) ? 0.7f : sDriveCamDist + 0.45f;
    if (!(nbIn.held & (BTN_CL | BTN_CR))) sDriveCamYaw *= (1.0f - nb_clampf(dt * 1.5f, 0, 1));

    yaw = vehicle_yaw(v) + sDriveCamYaw;
    dist = (260.0f + v->halfExtent[2] * 2.2f + v->halfExtent[0]) * sDriveCamDist;
    height = 110.0f + v->halfExtent[1] * 1.5f;

    v3_copy(target, v->x);
    target[1] += 50.0f + v->halfExtent[1];
    v3_set(fwd, 0, 0, 1);
    v3_rotateYaw(fwd, fwd, yaw);
    v3_set(off, -fwd[0] * dist, height * sDriveCamDist, -fwd[2] * dist);
    v3_add(eye, target, off);
    cam_collide(target, eye);

    if (!sDriveCamInit) {
        v3_copy(nbCamPos, eye);
        sDriveCamInit = TRUE;
    }
    k = nb_clampf(dt * 6.0f, 0.0f, 1.0f);
    nbCamPos[0] = nb_lerpf(nbCamPos[0], eye[0], k);
    nbCamPos[1] = nb_lerpf(nbCamPos[1], eye[1], k);
    nbCamPos[2] = nb_lerpf(nbCamPos[2], eye[2], k);
    v3_copy(eye, nbCamPos);
    cam_lookAt(eye, target);
}

/* ------------------------------------------------------------------ */
/* modes                                                               */

/* Banjo is in a real level and under the player's control */
static s32 playerInGame(void) {
    return player_is_present() && level_get() != 0 && level_get() != LEVEL_D_CUTSCENE;
}

static s32 canBuildHere(void) {
    return playerInGame()
        && player_getTransformation() == TRANSFORM_1_BANJO
        && player_isStable()
        && !player_inWater()
        && !player_isDead()
        && !gcdialog_hasCurrentTextId();
}

static void lockPlayer(s32 locked) {
    if (locked) {
        bs_setState(bs_getIdleState());
        sLockedState = bs_getState();
        func_8028F7C8(TRUE);
    } else {
        func_8028F7C8(FALSE);
    }
}

void nb_setMode(s32 mode) {
    s32 prev = nbMode;
    nbMode = mode;

    switch (mode) {
    case MODE_NORMAL:
        nbGarage.active = FALSE;
        nbVeh.driving = FALSE;
        nbCamActive = FALSE;
        player_setModelVisible(TRUE);
        lockPlayer(FALSE);
        break;
    case MODE_GARAGE:
        nbVeh.driving = FALSE;
        nbCamActive = TRUE;
        if (prev == MODE_NORMAL) lockPlayer(TRUE);
        player_setModelVisible(FALSE);
        break;
    case MODE_DRIVE:
        nbGarage.active = FALSE;
        nbVeh.driving = TRUE;
        nbCamActive = TRUE;
        sDriveCamInit = FALSE;
        sDriveCamYaw = 0.0f;
        if (prev == MODE_NORMAL) lockPlayer(TRUE);
        player_setModelVisible(TRUE);
        break;
    }
}

/* put Banjo back on his feet next to the vehicle */
void nb_exitVehicle(void) {
    NBVehicle *v = &nbVeh;
    f32 seat[3], side[3], start[3], end[3], n[3], pos[3];
    s32 i, found = FALSE;

    vehicle_seatWorld(v, seat);
    for (i = 0; i < 2 && !found; i++) {
        f32 s = (i == 0 ? 1.0f : -1.0f) * (v->halfExtent[0] + 55.0f);
        v3_copy(side, seat);
        side[0] += v->ax[0][0] * s;
        side[2] += v->ax[0][2] * s;
        v3_set(start, side[0], seat[1] + 150.0f, side[2]);
        v3_set(end, side[0], seat[1] - 800.0f, side[2]);
        if (func_80320B98(start, end, n, NB_FLOOR_FLAGS) != NULL && n[1] > 0.5f) {
            v3_set(pos, end[0], end[1] + 2.0f, end[2]);
            found = TRUE;
        }
    }
    if (!found) {
        v3_set(pos, seat[0], seat[1] + v->halfExtent[1] + 20.0f, seat[2]);
    }
    nb_setMode(MODE_NORMAL);
    func_8028F85C(pos);
    yaw_set(vehicle_yaw(v));
    yaw_setIdeal(vehicle_yaw(v));
    pitch_set(0.0f);
    roll_set(0.0f);
    gcsfx_playWithPitch(SFX_C7_SHWOOP, 1.1f, 26000);
}

static void normal_update(f32 dt) {
    f32 ppos[3], d[3];
    s32 near = FALSE;

    if (!playerInGame()) return;
    playerPosition_get(ppos);
    if (nbVeh.active) {
        v3_sub(d, ppos, nbVeh.x);
        near = (d[0] * d[0] + d[2] * d[2]) < 260.0f * 260.0f && d[1] > -250.0f && d[1] < 300.0f;
    }

    if (near) {
        hud_queueText(100, 168, "L: HOP IN", 255, 230, 120);
        hud_queueText(100, 182, "D-DOWN: EDIT", 255, 230, 120);
    } else if (sHintTimer < 6.0f) {
        sHintTimer += dt;
        hud_queueText(78, 182, "L: MUMBO'S GARAGE", 255, 230, 120);
    }

    if (nbIn.pressed & BTN_L) {
        if (near) {
            if (nbVeh.valid && !player_isDead()) {
                nb_setMode(MODE_DRIVE);
                gcsfx_playWithPitch(SFX_C7_SHWOOP, 0.9f, 26000);
            }
        } else if (canBuildHere()) {
            f32 floorPos[3];
            vehicle_despawn(&nbVeh);
            v3_copy(floorPos, ppos);
            garage_enter(floorPos, yaw_get());
        } else {
            nb_message("CAN'T BUILD HERE!", 1.5f);
            gcsfx_playWithPitch(SFX_CE_PAUSEMENU_HOIP, 0.8f, 22000);
        }
    } else if ((nbIn.pressed & BTN_DD) && near && canBuildHere()) {
        f32 floorPos[3], start[3], n[3];
        v3_copy(floorPos, nbVeh.x);
        v3_set(start, floorPos[0], floorPos[1] + 100.0f, floorPos[2]);
        floorPos[1] -= 800.0f;
        if (func_80320B98(start, floorPos, n, NB_FLOOR_FLAGS) == NULL) v3_copy(floorPos, ppos);
        {
            f32 yaw = vehicle_yaw(&nbVeh);
            vehicle_despawn(&nbVeh);
            garage_enter(floorPos, yaw);
        }
    }
}

static void drive_update(f32 dt) {
    NBVehicle *v = &nbVeh;
    f32 seat[3];

    if (!v->active) {
        nb_setMode(MODE_NORMAL);
        return;
    }
    if (nbIn.pressed & BTN_L) {
        nb_exitVehicle();
        return;
    }
    if (nbIn.pressed & BTN_DD) {
        f32 floorPos[3], start[3], n[3], yaw = vehicle_yaw(v);
        v3_copy(floorPos, v->x);
        v3_set(start, floorPos[0], floorPos[1] + 100.0f, floorPos[2]);
        floorPos[1] -= 800.0f;
        if (func_80320B98(start, floorPos, n, NB_FLOOR_FLAGS) == NULL) {
            v3_copy(floorPos, v->x);
            floorPos[1] -= v->halfExtent[1];
        }
        vehicle_despawn(v);
        garage_enter(floorPos, yaw);
        return;
    }

    vehicle_controls(v, dt);

    /* Banjo rides in the seat: sunk in so he looks seated */
    vehicle_seatWorld(v, seat);
    seat[1] -= 18.0f;
    playerPosition_set(seat);
    yaw_set(vehicle_yaw(v) + v->seatRot * 90.0f);
    {
        f32 pitch = RAD2DEG(nb_atan2f(-v->ax[2][1], nb_sqrtf(v->ax[2][0] * v->ax[2][0] + v->ax[2][2] * v->ax[2][2])));
        f32 roll = RAD2DEG(nb_atan2f(v->ax[0][1], v->ax[1][1]));
        pitch_set(pitch);
        roll_set(roll);
    }

    cam_driveUpdate(dt);
}

/* ------------------------------------------------------------------ */
/* hooks                                                               */

static void resetAll(void) {
    if (nbMode == MODE_GARAGE) player_setModelVisible(TRUE);
    vehicle_despawn(&nbVeh);
    nbGarage.active = FALSE;
    nbMode = MODE_NORMAL;
    nbCamActive = FALSE;
    nbMessageTimer = 0;
    sHintTimer = 0;
    {
        s32 i;
        for (i = 0; i < MAX_DEBRIS; i++) nbDebris[i].active = FALSE;
    }
}

static void nb_update(void) {
    f32 dt;

    if (getGameMode() != GAME_MODE_3_NORMAL || game_is_frozen()) {
        vehicle_soundStop(&nbVeh);
        return;
    }

    if (gsworld_getMap() != sMap) {
        resetAll();
        sMap = gsworld_getMap();
#ifdef NB_DEBUG
        item_set(ITEM_D_EGGS, 50); /* debug builds: eggs to test the cannon */
#endif
    }

    dt = nb_clampf(time_getDelta(), 0.0f, 0.1f);
    input_update();
    if (nbMessageTimer > 0) nbMessageTimer -= dt;

    /* Something in the game wants Banjo (jiggy jig, talking, getting hurt...):
     * hand him back so the game can run that state. */
    if (nbMode != MODE_NORMAL && bs_getState() != sLockedState) {
        if (nbMode == MODE_DRIVE) nb_exitVehicle();
        else {
            nbGarage.active = FALSE;
            nb_setMode(MODE_NORMAL);
        }
    }
    /* plain text boxes (tutorials etc.) just pause the mod until they close */
    if (gcdialog_hasCurrentTextId()) {
        vehicle_soundStop(&nbVeh);
        if (nbCamActive) {
            viewport_setPosition_vec3f(nbCamPos);
            viewport_setRotation_vec3f(nbCamRot);
            viewport_update();
        }
        return;
    }

    switch (nbMode) {
    case MODE_NORMAL: normal_update(dt); break;
    case MODE_GARAGE: garage_update(dt); break;
    case MODE_DRIVE:  drive_update(dt); break;
    }

    if (nbVeh.active) {
        if (nbMode != MODE_DRIVE) {
            nbVeh.throttle = 0.0f;
            nbVeh.steer = 0.0f;
        }
        vehicle_step(&nbVeh, dt);
        if (nbMode != MODE_DRIVE) vehicle_soundStop(&nbVeh);
    }
    debris_update(dt);

    if (nbMode == MODE_GARAGE) garage_camera(dt);

#ifdef NB_DEBUG
    {
        static char dbg[40];
        char num[12];
        dbg[0] = 0;
        nb_strcat(dbg, "M"); nb_itoa(num, nbMode); nb_strcat(dbg, num);
        nb_strcat(dbg, " A"); nb_itoa(num, nbVeh.active); nb_strcat(dbg, num);
        nb_strcat(dbg, " G"); nb_itoa(num, nbVeh.onGround); nb_strcat(dbg, num);
        nb_strcat(dbg, " S"); nb_itoa(num, bs_getState()); nb_strcat(dbg, num);
        nb_strcat(dbg, " Y"); nb_itoa(num, (s32)nbVeh.x[1]); nb_strcat(dbg, num);
        nb_strcat(dbg, " X"); nb_itoa(num, (s32)nbVeh.x[0]); nb_strcat(dbg, num);
        nb_strcat(dbg, " Z"); nb_itoa(num, (s32)nbVeh.x[2]); nb_strcat(dbg, num);
        nb_strcat(dbg, " MS"); nb_itoa(num, (s32)(time_getDelta() * 1000.0f)); nb_strcat(dbg, num);
        hud_queueText(12, 50, dbg, 120, 255, 120);
        dbg[0] = 0;
        nb_strcat(dbg, "W"); nb_itoa(num, nbVeh.dbgWheels); nb_strcat(dbg, num);
        nb_strcat(dbg, " H"); nb_itoa(num, nbVeh.dbgHull); nb_strcat(dbg, num);
        nb_strcat(dbg, " V"); nb_itoa(num, (s32)nbVeh.speed); nb_strcat(dbg, num);
        nb_strcat(dbg, " T"); nb_itoa(num, (s32)(nbVeh.throttle * 100)); nb_strcat(dbg, num);
        nb_strcat(dbg, " UP"); nb_itoa(num, (s32)(nbVeh.ax[1][1] * 100)); nb_strcat(dbg, num);
        nb_strcat(dbg, " WTR"); nb_itoa(num, nbVeh.inWater ? (s32)nbVeh.waterY : -1); nb_strcat(dbg, num);
        hud_queueText(12, 64, dbg, 120, 255, 120);
    }
#endif
    if (nbMessageTimer > 0) {
        s32 len = 0;
        while (nbMessage[len]) len++;
        hud_queueText(SCREEN_W / 2 - len * 3, 100, nbMessage, 255, 120, 80);
    }
    hud_flushText();
}

static void nb_drawWorld(Gfx **gfx, Mtx **mtx, Vtx **vtx) {
    render_world(gfx, mtx, vtx);
}

static void nb_drawHud(Gfx **gfx, Mtx **mtx, Vtx **vtx) {
    render_hud(gfx, mtx, vtx);
}

static s32 nb_blockPause(void) {
    return nbMode != MODE_NORMAL;
}

static void nb_mapUnload(void) {
    resetAll();
    sMap = -1;
}
