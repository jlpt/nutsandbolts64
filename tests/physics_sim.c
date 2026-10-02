/*
 * Host-side physics sandbox for the Nuts & Bolts 64 vehicle code.
 *
 * Compiles the real nb_vehicle.c / nb_parts.c / nb_math.c against stubbed
 * Banjo-Kazooie functions and a synthetic world (flat ground, a ramp, a cliff
 * and a lake), then drives the preset vehicles through scripted scenarios
 * and checks the results. Run with `make test` from the repository root.
 */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "nb.h"

/* ---------------- synthetic world ---------------- */
/*   z in [3000, 4500]: 22 degree ramp up to a plateau at +606
 *   x < -4000: cliff wall 600 tall
 *   x > 6000: lake, bed at -400, water surface at 0          */
static f32 ground(f32 x, f32 z) {
    f32 h = 0.0f;
    if (x > 6000.0f) return -400.0f;
    if (z > 3000.0f) h = (z < 4500.0f ? (z - 3000.0f) : 1500.0f) * 0.404f;
    if (x < -4000.0f) h = 600.0f;
    return h;
}

static void groundNormal(f32 x, f32 z, f32 n[3]) {
    f32 e = 4.0f;
    f32 dx = ground(x + e, z) - ground(x - e, z);
    f32 dz = ground(x, z + e) - ground(x, z - e);
    v3_set(n, -dx, 2 * e, -dz);
    v3_normalize(n);
}

static BKCollisionTriangle sGroundTri = {{0, 0, 0}, 0, 0};
static BKCollisionTriangle sWaterTri = {{0, 0, 0}, 0, COLL_FLAG_WATER};
s32 gRaycasts;

BKCollisionTriangle *func_80320B98(f32 start[3], f32 end[3], f32 normal[3], u32 flags) {
    s32 i;
    f32 a = 0, b = 1, p[3];
    gRaycasts++;
    if (start[1] < ground(start[0], start[2])) return NULL; /* back-facing */
    for (i = 1; i <= 48; i++) {
        f32 t = i / 48.0f;
        p[0] = start[0] + (end[0] - start[0]) * t;
        p[1] = start[1] + (end[1] - start[1]) * t;
        p[2] = start[2] + (end[2] - start[2]) * t;
        if (p[1] < ground(p[0], p[2])) { b = t; a = (i - 1) / 48.0f; break; }
    }
    if (i > 48) return NULL;
    for (i = 0; i < 20; i++) {
        f32 m = (a + b) * 0.5f;
        p[0] = start[0] + (end[0] - start[0]) * m;
        p[1] = start[1] + (end[1] - start[1]) * m;
        p[2] = start[2] + (end[2] - start[2]) * m;
        if (p[1] < ground(p[0], p[2])) b = m; else a = m;
    }
    end[0] = start[0] + (end[0] - start[0]) * a;
    end[1] = start[1] + (end[1] - start[1]) * a;
    end[2] = start[2] + (end[2] - start[2]) * a;
    groundNormal(end[0], end[2], normal);
    /* a vertical step: make the normal horizontal */
    if (fabsf(ground(end[0] + 6, end[2]) - ground(end[0] - 6, end[2])) > 100) v3_set(normal, end[0] < -4000 ? -1 : 1, 0, 0);
    return &sGroundTri;
}

BKCollisionTriangle *mapModel_intersectLine(f32 start[3], f32 end[3], f32 normal[3], s32 flags) {
    if (start[0] > 6000.0f && start[1] > 0.0f && end[1] < 0.0f) {
        f32 t = start[1] / (start[1] - end[1]);
        end[0] = start[0] + (end[0] - start[0]) * t;
        end[1] = 0.0f;
        end[2] = start[2] + (end[2] - start[2]) * t;
        v3_set(normal, 0, 1, 0);
        return &sWaterTri;
    }
    return func_80320B98(start, end, normal, flags);
}

/* ---------------- stubs ---------------- */
NBInput nbIn;
NBBlueprint nbBlueprint;
s32 nbMode = MODE_DRIVE;
static f32 sPlayer[3];
void gcsfx_playWithPitch(s32 sfx, f32 pitch, s32 volume) {}
u8 sfxsource_createSfxsourceAndReturnIndex(void) { return 0; }
void sfxsource_freeSfxsourceByIndex(u8 idx) {}
void sfxsource_setSfxId(u8 idx, s32 sfx) {}
void sfxsource_setSampleRate(u8 idx, s32 volume) {}
void sfxsource_playSfxAtVolume(u8 idx, f32 pitch) {}
void sfxSource_setunk43_7ByIndex(u8 idx, s32 mode) {}
void sfxSource_func_8030E2C4(u8 idx) {}
void sfxsource_set_fade_distances(u8 idx, f32 min, f32 max) {}
void sfxsource_set_position(u8 idx, f32 pos[3]) {}
s32 item_getCount(s32 item) { return 99; }
void item_dec(s32 item) {}
s32 item_empty(s32 item) { return 0; }
s32 commonParticle_new(s32 id, s32 arg) { return 0; }
void playerPosition_get(f32 pos[3]) { v3_copy(pos, sPlayer); }
void playerPosition_set(f32 pos[3]) { v3_copy(sPlayer, pos); }
void yaw_set(f32 d) {}
void nb_message(const char *msg, f32 s) { printf("      [msg] %s\n", msg); }
void nb_exitVehicle(void) { printf("      [exit vehicle]\n"); }
void osWritebackDCache(void *p, s32 n) {}

/* ---------------- harness ---------------- */
static s32 sFails;

static void check(s32 ok, const char *what) {
    printf("    %s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) sFails++;
}

static s32 fin(f32 x) {
    volatile f32 d = x - x;
    return x == x && d == 0.0f;
}

static s32 finite3(const f32 v[3]) {
    return fin(v[0]) && fin(v[1]) && fin(v[2]);
}

static NBVehicle *spawn(s32 preset, f32 x, f32 y, f32 z, f32 yaw) {
    NBVehicle *v = &nbVeh;
    f32 pos[3] = {x, y, z};
    s32 i;
    memset(v, 0, sizeof(*v));
    preset_build(preset, &nbBlueprint);
    if (vehicle_compile(v, &nbBlueprint) <= 0) {
        printf("    compile failed\n");
        sFails++;
        return v;
    }
    for (i = 0; i < nbBlueprint.count; i++) v->hp[i] = nbPartDefs[nbBlueprint.parts[i].type].hp;
    vehicle_spawn(v, pos, yaw);
    v->driving = TRUE;
    return v;
}

static void run(NBVehicle *v, f32 seconds, u16 buttons, f32 sx, f32 sy) {
    f32 t;
    u16 prev = 0;
    for (t = 0; t < seconds; t += 1.0f / 30.0f) {
        nbIn.held = buttons;
        nbIn.pressed = buttons & ~prev;
        prev = buttons;
        nbIn.sx = sx;
        nbIn.sy = sy;
        vehicle_controls(v, 1.0f / 30.0f);
        vehicle_step(v, 1.0f / 30.0f);
        debris_update(1.0f / 30.0f);
        if (!finite3(v->x) || !finite3(v->v) || !finite3(v->w)) {
            printf("    NaN!\n");
            sFails++;
            return;
        }
    }
}

static void report(const char *label, NBVehicle *v) {
    printf("    %-22s pos (%7.0f %6.0f %7.0f) speed %5.0f up %.2f yaw %6.1f fuel %4.0f ground %d\n",
           label, v->x[0], v->x[1], v->x[2], v->speed, v->ax[1][1], vehicle_yaw(v), v->fuel, v->onGround);
}

static f32 height(NBVehicle *v) {
    return v->x[1] - ground(v->x[0], v->x[2]);
}

int main(void) {
    NBVehicle *v;
    f32 z0, yaw0;

    printf("== TROLLEY: flat ground ==\n");
    v = spawn(0, 0, 0, 0, 0);
    report("spawn", v);
    run(v, 2.0f, 0, 0, 0);
    report("settled", v);
    check(fabsf(v->speed) < 20.0f && v->ax[1][1] > 0.98f, "comes to rest upright");
    check(v->dbgHull == 0, "body clears the ground on its wheels");
    z0 = v->x[2];
    run(v, 4.0f, BTN_A, 0, 0);
    report("4s throttle", v);
    check(v->speed > 700.0f, "accelerates to a decent speed");
    check(v->x[2] - z0 > 1500.0f && fabsf(v->x[0]) < 200.0f, "drives straight forward");
    yaw0 = vehicle_yaw(v);
    run(v, 1.0f, BTN_A, 1.0f, 0);
    report("1s steer right", v);
    check(nb_wrapDeg(vehicle_yaw(v) - yaw0) < -45.0f && nb_wrapDeg(vehicle_yaw(v) - yaw0) > -170.0f, "steering right turns right (yaw decreases)");
    check(v->ax[1][1] > 0.8f, "doesn't roll over in a turn");
    run(v, 1.2f, BTN_B, 0, 0);
    report("1.2s brake", v);
    check(v->speed < 150.0f, "brakes");

    printf("== TROLLEY: 22 degree ramp ==\n");
    v = spawn(0, 0, 0, 2400, 0);
    run(v, 1.0f, 0, 0, 0);
    run(v, 7.0f, BTN_A, 0, 0);
    report("7s up the ramp", v);
    check(v->x[2] > 4600.0f, "climbs the ramp onto the plateau");

    printf("== KAZOOIE KART: top speed ==\n");
    v = spawn(1, 0, 0, -6000, 0);
    run(v, 1.0f, 0, 0, 0);
    run(v, 5.0f, BTN_A, 0, 0);
    report("5s throttle", v);
    check(v->speed > 1100.0f, "kart is quicker than the trolley");

    printf("== MONSTER TRUCK: ramp + boost ==\n");
    v = spawn(5, 0, 0, 2200, 0);
    run(v, 1.0f, 0, 0, 0);
    report("settled", v);
    check(v->ax[1][1] > 0.98f, "monster truck sits upright");
    run(v, 6.0f, BTN_A | BTN_Z, 0, 0);
    report("6s throttle+boost", v);
    check(v->x[2] > 4600.0f, "climbs the ramp");

    printf("== GLIDER: take-off ==\n");
    v = spawn(2, -2000, 0, -8000, 0);
    run(v, 1.0f, 0, 0, 0);
    report("settled", v);
    check(v->ax[1][1] > 0.98f, "glider sits level on its landing gear");
    {
        f32 maxH = 0, t;
        for (t = 0; t < 8.0f; t += 0.5f) {
            run(v, 0.5f, BTN_A, 0, 0);
            if (height(v) > maxH) maxH = height(v);
        }
        report("8s throttle", v);
        printf("    max height %.0f\n", maxH);
        check(maxH > 200.0f, "takes off by itself at speed");
    }
    run(v, 0.5f, BTN_A, 0, -0.6f);
    run(v, 3.0f, BTN_A, 0, 0);
    report("pull up, then let go", v);
    check(v->ax[1][1] > 0.7f && height(v) > 100.0f, "stays up and recovers");
    yaw0 = vehicle_yaw(v);
    run(v, 2.0f, BTN_A, 0.7f, 0);
    report("2s bank right", v);
    check(nb_wrapDeg(vehicle_yaw(v) - yaw0) < -20.0f, "banks into a right turn");
    run(v, 2.0f, BTN_A, 0, 0);
    report("2s let go", v);
    check(v->ax[1][1] > 0.8f, "levels out after the turn");

    printf("== BOAT: on the lake ==\n");
    v = spawn(3, 8000, 20, 0, 90);
    run(v, 4.0f, 0, 0, 0);
    report("4s floating", v);
    check(v->inWater && v->x[1] > -40.0f && v->x[1] < 60.0f, "floats at the surface");
    check(v->ax[1][1] > 0.95f, "floats upright");
    z0 = v->x[0];
    run(v, 5.0f, BTN_A, 0, 0);
    report("5s propeller", v);
    check(v->x[0] - z0 > 800.0f, "propeller pushes it along");

    printf("== BALLOON BUS: hover ==\n");
    v = spawn(4, 2000, 0, -2000, 0);
    run(v, 10.0f, 0, 0, 0);
    report("10s", v);
    check(height(v) > 250.0f && height(v) < 1300.0f, "hovers at a sensible height");
    check(v->ax[1][1] > 0.9f, "stays level");

    printf("== TROLLEY: drop + upside-down recovery ==\n");
    v = spawn(0, -1000, 400, 0, 0);
    run(v, 3.0f, 0, 0, 0);
    report("dropped from 400", v);
    check(v->ax[1][1] > 0.9f && height(v) < 120.0f, "lands on its wheels");
    v3_scale(v->ax[1], v->ax[1], -1.0f);
    v3_scale(v->ax[0], v->ax[0], -1.0f);
    v->x[1] += 80.0f;
    run(v, 4.0f, 0, 0, 0);
    report("left upside down", v);
    check(v->ax[1][1] > 0.9f, "auto-flips upright after a while");

    printf("== TROLLEY: wall impact ==\n");
    v = spawn(0, -3200, 0, 0, -90);
    run(v, 1.0f, 0, 0, 0);
    run(v, 4.0f, BTN_A, 0, 0);
    report("into the cliff", v);
    check(v->x[0] > -4060.0f, "doesn't pass through the wall");

    {
        s32 before;
        v = spawn(0, 0, 0, 0, 0);
        run(v, 1.0f, 0, 0, 0);
        before = gRaycasts;
        run(v, 1.0f, BTN_A, 0, 0);
        printf("raycasts per frame while driving the trolley: %.1f\n", (gRaycasts - before) / 30.0f);
    }
    printf("\n%s (%d failures)\n", sFails ? "SOME CHECKS FAILED" : "ALL CHECKS PASSED", sFails);
    return sFails ? 1 : 0;
}
