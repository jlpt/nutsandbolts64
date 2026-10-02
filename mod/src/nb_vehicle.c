/*
 * Vehicle compilation and physics.
 *
 * A blueprint is compiled into a rigid body (mass, centre of mass, inertia)
 * plus lists of functional parts. The body is simulated with a small
 * fixed-step integrator against Banjo-Kazooie's own collision:
 *   - wheels: raycast suspension + tyre friction (like a "raycast vehicle")
 *   - hull: support points of the body that push out of floors and walls
 *   - wings/fins: simple lift and side-force model
 *   - propellers/jets: thrust at their mount point
 *   - floaters, balloons: buoyancy / lift
 * Heavy hits damage parts, and broken parts fall off as debris.
 */
#include "nb.h"

NBVehicle nbVeh;
NBDebris nbDebris[MAX_DEBRIS];

#define GRAVITY      2200.0f
#define DRIVE_K      62000.0f
#define PROP_K       30000.0f
#define JET_K        16000.0f
#define LIFT_K       0.03f
#define CAMBER_K     0.0090f
#define FIN_K        0.03f
#define BALLOON_LIFT 8.0f
#define FLOAT_LIFT   9.0f
#define DRAG_Q       0.00062f
#define DRAG_L       0.05f
#define SUBSTEP      (1.0f / 60.0f)

/* ------------------------------------------------------------------ */
/* compile                                                             */

static s8 sOcc[GRID_N][GRID_H][GRID_N];

static void partForward(s32 rot, f32 out[3]) {
    static const f32 dirs[4][3] = {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}};
    v3_copy(out, dirs[rot & 3]);
}

static void partRight(s32 rot, f32 out[3]) {
    /* local X axis of the rotated part */
    static const f32 dirs[4][3] = {{1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 0, 1}};
    v3_copy(out, dirs[rot & 3]);
}

static void addHull(NBVehicle *v, const f32 p[3]) {
    s32 i;
    f32 d[3];
    for (i = 0; i < v->numHull; i++) {
        v3_sub(d, v->hull[i], p);
        if (v3_dot(d, d) < 25.0f) return;
    }
    if (v->numHull < MAX_HULL) v3_copy(v->hull[v->numHull++], p);
}

s32 vehicle_compile(NBVehicle *v, const NBBlueprint *bp) {
    static u8 queue[MAX_PARTS];
    static u8 reach[MAX_PARTS];
    s32 i, j, x, y, z, head = 0, tail = 0, seat = -1, n = 0;
    f32 cm[3] = {0, 0, 0}, mass = 0;
    f32 mn[3] = {1e9f, 1e9f, 1e9f}, mx[3] = {-1e9f, -1e9f, -1e9f};

    v->valid = FALSE;
    v->numWheels = v->numThrust = v->numWings = v->numFins = 0;
    v->numFloat = v->numHull = v->numBalloons = 0;
    v->numSprings = v->numCannons = v->numHorns = v->numJets = v->numProps = v->numGyros = 0;
    v->numCannonPos = 0;
    v->enginePower = 0;
    v->fuelMax = 0;

    /* occupancy grid */
    nb_memset(sOcc, -1, sizeof(sOcc));
    for (i = 0; i < bp->count; i++) {
        const NBPart *p = &bp->parts[i];
        s32 sx, sy, sz;
        reach[i] = 0;
        if (v->broken[i]) continue;
        bp_partExtent(p, &sx, &sy, &sz);
        for (x = 0; x < sx; x++)
            for (y = 0; y < sy; y++)
                for (z = 0; z < sz; z++) {
                    s32 gx = p->x + x + GRID_HALF, gy = p->y + y, gz = p->z + z + GRID_HALF;
                    if (gx >= 0 && gx < GRID_N && gy >= 0 && gy < GRID_H && gz >= 0 && gz < GRID_N)
                        sOcc[gx][gy][gz] = i;
                }
        if (seat < 0 && nbPartDefs[p->type].kind == KIND_SEAT) seat = i;
    }
    if (seat < 0) return -1;

    /* flood fill from the seat: only attached parts count */
    queue[tail++] = seat;
    reach[seat] = 1;
    while (head < tail) {
        const NBPart *p = &bp->parts[queue[head++]];
        s32 sx, sy, sz;
        bp_partExtent(p, &sx, &sy, &sz);
        for (x = -1; x <= sx; x++)
            for (y = -1; y <= sy; y++)
                for (z = -1; z <= sz; z++) {
                    s32 out = (x < 0 || x >= sx) + (y < 0 || y >= sy) + (z < 0 || z >= sz);
                    s32 gx = p->x + x + GRID_HALF, gy = p->y + y, gz = p->z + z + GRID_HALF, o;
                    if (out != 1) continue;
                    if (gx < 0 || gx >= GRID_N || gy < 0 || gy >= GRID_H || gz < 0 || gz >= GRID_N) continue;
                    o = sOcc[gx][gy][gz];
                    if (o >= 0 && !reach[o]) {
                        reach[o] = 1;
                        queue[tail++] = o;
                    }
                }
    }

    /* mass properties */
    for (i = 0; i < bp->count; i++) {
        f32 c[3];
        if (!reach[i]) continue;
        bp_partCenter(&bp->parts[i], c);
        mass += nbPartDefs[bp->parts[i].type].mass;
        v3_addScaled(cm, c, nbPartDefs[bp->parts[i].type].mass);
        n++;
    }
    if (mass <= 0) return -1;
    v3_scale(cm, cm, 1.0f / mass);
    v3_copy(v->cm, cm);
    v->mass = mass;
    v->inertia[0] = v->inertia[1] = v->inertia[2] = mass * 400.0f;

    for (i = 0; i < bp->count; i++) {
        const NBPart *p = &bp->parts[i];
        const NBPartDef *d = &nbPartDefs[p->type];
        f32 c[3], rel[3], fw[3], rt[3], he[3];
        s32 sx, sy, sz;
        if (!reach[i]) continue;

        bp_partCenter(p, c);
        v3_sub(rel, c, cm);
        bp_partExtent(p, &sx, &sy, &sz);
        v3_set(he, sx * CELL * 0.5f, sy * CELL * 0.5f, sz * CELL * 0.5f);
        partForward(p->rot, fw);
        partRight(p->rot, rt);

        v->inertia[0] += d->mass * (rel[1] * rel[1] + rel[2] * rel[2] + (he[1] * he[1] + he[2] * he[2]) / 3.0f);
        v->inertia[1] += d->mass * (rel[0] * rel[0] + rel[2] * rel[2] + (he[0] * he[0] + he[2] * he[2]) / 3.0f);
        v->inertia[2] += d->mass * (rel[0] * rel[0] + rel[1] * rel[1] + (he[0] * he[0] + he[1] * he[1]) / 3.0f);

        for (j = 0; j < 3; j++) {
            if (rel[j] - he[j] < mn[j]) mn[j] = rel[j] - he[j];
            if (rel[j] + he[j] > mx[j]) mx[j] = rel[j] + he[j];
        }

        switch (d->kind) {
        case KIND_WHEEL:
        case KIND_SKI:
        case KIND_TREAD: {
            s32 k, cnt = (d->kind == KIND_TREAD) ? 2 : 1;
            for (k = 0; k < cnt && v->numWheels < MAX_WHEELS; k++) {
                NBWheel *w = &v->wheels[v->numWheels++];
                v3_copy(w->pos, rel);
                if (d->kind == KIND_TREAD) v3_addScaled(w->pos, fw, k == 0 ? CELL : -CELL);
                if (d->kind == KIND_SKI) w->pos[1] -= CELL * 0.5f - 4.0f;
                w->radius = (d->kind == KIND_SKI) ? 4.0f : d->value;
                w->grip = (d->kind == KIND_TREAD) ? 1.6f : 1.0f;
                w->ski = (d->kind == KIND_SKI);
                w->spin = 0;
                w->compression = 0;
                w->contact = 0;
                w->steer = 0;
                w->part = i;
            }
            break;
        }
        case KIND_ENGINE: v->enginePower += d->value; break;
        case KIND_FUEL:   v->fuelMax += d->value; break;
        case KIND_SEAT:
            if (i == seat) {
                v3_copy(v->seat, rel);
                v->seat[1] += CELL * 0.5f;
                v->seatPart = i;
                v->seatRot = p->rot;
            }
            break;
        case KIND_PROP:
        case KIND_JET:
            if (v->numThrust < MAX_THRUST) {
                NBThruster *t = &v->thrust[v->numThrust++];
                v3_copy(t->pos, rel);
                v3_copy(t->dir, fw);
                t->jet = (d->kind == KIND_JET);
                t->force = d->value * (t->jet ? JET_K : PROP_K);
                t->part = i;
            }
            if (d->kind == KIND_JET) v->numJets++; else v->numProps++;
            break;
        case KIND_WING:
            if (v->numWings < MAX_AERO) {
                NBAero *a = &v->wings[v->numWings++];
                v3_copy(a->pos, rel);
                v3_set(a->axis, 0, 1, 0);
                a->area = d->value;
            }
            break;
        case KIND_FIN:
            if (v->numFins < MAX_AERO) {
                NBAero *a = &v->fins[v->numFins++];
                v3_copy(a->pos, rel);
                v3_copy(a->axis, rt);
                a->area = d->value;
            }
            break;
        case KIND_FLOATER:
            if (v->numFloat < MAX_FLOAT) v3_copy(v->floaters[v->numFloat++], rel);
            break;
        case KIND_BALLOON:
            if (v->numBalloons < MAX_FLOAT) {
                v3_copy(v->balloons[v->numBalloons], rel);
                v->balloons[v->numBalloons][1] += CELL * 0.5f;
                v->numBalloons++;
            }
            break;
        case KIND_SPRING: v->numSprings++; break;
        case KIND_GYRO:   v->numGyros++; break;
        case KIND_HORN:   v->numHorns++; break;
        case KIND_CANNON:
            v->numCannons++;
            if (v->numCannonPos < 8) {
                f32 *cp = v->cannons[v->numCannonPos++];
                cp[0] = rel[0] + fw[0] * CELL;
                cp[1] = rel[1];
                cp[2] = rel[2] + fw[2] * CELL;
                cp[3] = p->rot * 90.0f;
            }
            break;
        }
    }

    /* hull support points from every non-wheel part's box corners */
    {
        static const f32 dirs[14][3] = {
            {1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1},
            {1, 1, 1}, {1, 1, -1}, {1, -1, 1}, {1, -1, -1},
            {-1, 1, 1}, {-1, 1, -1}, {-1, -1, 1}, {-1, -1, -1},
        };
        s32 k;
        for (k = 0; k < 14; k++) {
            f32 best = -1e9f, bestP[3] = {0, 0, 0};
            s32 found = FALSE;
            for (i = 0; i < bp->count; i++) {
                const NBPart *p = &bp->parts[i];
                const NBPartDef *d = &nbPartDefs[p->type];
                f32 c[3], he[3], corner[3];
                s32 sx, sy, sz, cx, cy, cz;
                if (!reach[i] || d->kind == KIND_WHEEL || d->kind == KIND_TREAD) continue;
                bp_partCenter(p, c);
                v3_sub(c, c, cm);
                bp_partExtent(p, &sx, &sy, &sz);
                v3_set(he, sx * CELL * 0.5f - 2.0f, sy * CELL * 0.5f - 2.0f, sz * CELL * 0.5f - 2.0f);
                for (cx = -1; cx <= 1; cx += 2)
                    for (cy = -1; cy <= 1; cy += 2)
                        for (cz = -1; cz <= 1; cz += 2) {
                            f32 dd;
                            v3_set(corner, c[0] + cx * he[0], c[1] + cy * he[1], c[2] + cz * he[2]);
                            dd = v3_dot(corner, dirs[k]);
                            if (dd > best) { best = dd; v3_copy(bestP, corner); found = TRUE; }
                        }
            }
            if (found) addHull(v, bestP);
        }
        /* a wheel-only contraption still needs something to stand on */
        if (v->numHull == 0) {
            f32 p[3] = {0, mn[1], 0};
            addHull(v, p);
        }
    }

    for (j = 0; j < 3; j++) v->halfExtent[j] = (mx[j] - mn[j]) * 0.5f;

    /* front wheels steer */
    {
        f32 maxZ = -1e9f, minZ = 1e9f;
        for (i = 0; i < v->numWheels; i++) {
            if (v->wheels[i].pos[2] > maxZ) maxZ = v->wheels[i].pos[2];
            if (v->wheels[i].pos[2] < minZ) minZ = v->wheels[i].pos[2];
        }
        for (i = 0; i < v->numWheels; i++) {
            if (maxZ - minZ > CELL * 0.9f && v->wheels[i].pos[2] > (maxZ + minZ) * 0.5f + 1.0f)
                v->wheels[i].steer = 1;
        }
    }

    v->valid = TRUE;
    return n;
}

/* ------------------------------------------------------------------ */

void vehicle_localToWorld(NBVehicle *v, const f32 l[3], f32 out[3]) {
    s32 i;
    for (i = 0; i < 3; i++)
        out[i] = v->x[i] + v->ax[0][i] * l[0] + v->ax[1][i] * l[1] + v->ax[2][i] * l[2];
}

static void localDirToWorld(NBVehicle *v, const f32 l[3], f32 out[3]) {
    s32 i;
    for (i = 0; i < 3; i++)
        out[i] = v->ax[0][i] * l[0] + v->ax[1][i] * l[1] + v->ax[2][i] * l[2];
}

void vehicle_seatWorld(NBVehicle *v, f32 out[3]) {
    vehicle_localToWorld(v, v->seat, out);
}

f32 vehicle_yaw(NBVehicle *v) {
    f32 fx = v->ax[2][0], fz = v->ax[2][2];
    if (fx * fx + fz * fz < 0.01f) {
        fx = -v->ax[1][0];
        fz = -v->ax[1][2];
    }
    return RAD2DEG(nb_atan2f(fx, fz));
}

static void setYawAxes(NBVehicle *v, f32 yaw) {
    f32 r = DEG2RAD(yaw), c = cosf(r), s = sinf(r);
    v3_set(v->ax[0], c, 0, -s);
    v3_set(v->ax[1], 0, 1, 0);
    v3_set(v->ax[2], s, 0, c);
}

void vehicle_soundStop(NBVehicle *v) {
    if (v->engineSfx) {
        sfxsource_freeSfxsourceByIndex(v->engineSfx);
        v->engineSfx = 0;
    }
}

void vehicle_spawn(NBVehicle *v, const f32 floorPos[3], f32 yaw) {
    f32 origin[3], lift = 4.0f, cmw[3];
    s32 i;

    for (i = 0; i < v->numWheels; i++) {
        f32 bottom = v->wheels[i].pos[1] + v->cm[1] - v->wheels[i].radius;
        if (-bottom + 4.0f > lift) lift = -bottom + 4.0f;
    }
    v3_copy(origin, floorPos);
    origin[1] += lift;
    setYawAxes(v, yaw);
    localDirToWorld(v, v->cm, cmw);
    v3_add(v->x, origin, cmw);
    v3_set(v->v, 0, 0, 0);
    v3_set(v->w, 0, 0, 0);
    v->fuel = v->fuelMax;
    v->active = TRUE;
    v->throttle = v->steer = 0;
    v->airTime = v->upsideTime = 0;
    v->groundTime = 0;
    v3_copy(v->safePos, v->x);
    v->safeYaw = yaw;
    v->safeTimer = 0;
    v->cannonCooldown = v->springCooldown = v->hornCooldown = 0;
    v->needsRecompile = FALSE;
    v->waterY = -100000.0f;
    v->inWater = FALSE;
}

void vehicle_despawn(NBVehicle *v) {
    vehicle_soundStop(v);
    v->active = FALSE;
    v->driving = FALSE;
}

void vehicle_flipUpright(NBVehicle *v) {
    f32 yaw = vehicle_yaw(v);
    setYawAxes(v, yaw);
    v->x[1] += v->halfExtent[1] + 40.0f;
    v3_set(v->w, 0, 0, 0);
    v->v[1] = 0;
    gcsfx_playWithPitch(SFX_30_MAGIC_POOF, 1.0f, 24000);
}

/* ------------------------------------------------------------------ */
/* damage & debris                                                     */

static void spawnDebris(NBVehicle *v, s32 part) {
    const NBPart *p = &nbBlueprint.parts[part];
    f32 c[3], rel[3];
    s32 i;
    for (i = 0; i < MAX_DEBRIS; i++) {
        NBDebris *d = &nbDebris[i];
        if (d->active) continue;
        bp_partCenter(p, c);
        v3_sub(rel, c, v->cm);
        vehicle_localToWorld(v, rel, d->pos);
        v3_copy(d->vel, v->v);
        v3_addScaled(d->vel, v->ax[1], 400.0f);
        v3_copy(d->ax[0], v->ax[0]);
        v3_copy(d->ax[1], v->ax[1]);
        v3_copy(d->ax[2], v->ax[2]);
        v3_set(d->spin, 3.0f + (i % 3), 2.0f, 4.0f - (i % 2));
        d->timer = 3.0f;
        d->type = p->type;
        d->color = p->color;
        d->rot = p->rot;
        d->active = TRUE;
        return;
    }
}

static void damageNear(NBVehicle *v, const f32 localPoint[3], s32 amount) {
    s32 i, best = -1;
    f32 bestD = 1e9f;
    for (i = 0; i < nbBlueprint.count; i++) {
        f32 c[3], d[3], dd;
        s32 kind = nbPartDefs[nbBlueprint.parts[i].type].kind;
        if (v->broken[i] || kind == KIND_WHEEL) continue;
        bp_partCenter(&nbBlueprint.parts[i], c);
        v3_sub(c, c, v->cm);
        v3_sub(d, c, localPoint);
        dd = v3_dot(d, d);
        if (dd < bestD) { bestD = dd; best = i; }
    }
    if (best < 0) return;
    v->hp[best] -= amount;
    if (v->hp[best] <= 0) {
        if (best == v->seatPart) {
            v->hp[best] = 1; /* the seat never breaks off; it just takes a beating */
            return;
        }
        v->broken[best] = 1;
        v->needsRecompile = TRUE;
        spawnDebris(v, best);
        gcsfx_playWithPitch(nbPartDefs[nbBlueprint.parts[best].type].mass > 3.0f ? SFX_82_METAL_BREAK : SFX_D9_WOODEN_CRATE_BREAKING_1, 1.0f, 28000);
    }
}

void debris_update(f32 dt) {
    s32 i, j;
    for (i = 0; i < MAX_DEBRIS; i++) {
        NBDebris *d = &nbDebris[i];
        if (!d->active) continue;
        d->timer -= dt;
        if (d->timer <= 0) { d->active = FALSE; continue; }
        d->vel[1] -= GRAVITY * dt;
        v3_addScaled(d->pos, d->vel, dt);
        {
            f32 start[3], end[3], n[3];
            v3_set(start, d->pos[0], d->pos[1] + 40.0f, d->pos[2]);
            v3_set(end, d->pos[0], d->pos[1] - 15.0f, d->pos[2]);
            if (func_80320B98(start, end, n, NB_FLOOR_FLAGS) != NULL && d->vel[1] < 0) {
                d->pos[1] = end[1] + 15.0f;
                d->vel[1] *= -0.35f;
                d->vel[0] *= 0.6f;
                d->vel[2] *= 0.6f;
                v3_scale(d->spin, d->spin, 0.5f);
            }
        }
        for (j = 0; j < 3; j++) {
            f32 dw[3];
            v3_cross(dw, d->spin, d->ax[j]);
            v3_addScaled(d->ax[j], dw, dt);
        }
        v3_normalize(d->ax[2]);
        v3_cross(d->ax[0], d->ax[1], d->ax[2]);
        v3_normalize(d->ax[0]);
        v3_cross(d->ax[1], d->ax[2], d->ax[0]);
    }
}

/* ------------------------------------------------------------------ */
/* physics                                                             */

static f32 sF[3], sT[3];

static void applyForce(NBVehicle *v, const f32 f[3], const f32 at[3]) {
    f32 r[3], t[3];
    v3_add(sF, sF, f);
    v3_sub(r, at, v->x);
    v3_cross(t, r, f);
    v3_add(sT, sT, t);
}

static void pointVel(NBVehicle *v, const f32 p[3], f32 out[3]) {
    f32 r[3], wr[3];
    v3_sub(r, p, v->x);
    v3_cross(wr, v->w, r);
    v3_add(out, v->v, wr);
}

/* world-space inverse inertia applied to a vector (diagonal in body space) */
static void applyInvI(NBVehicle *v, const f32 in[3], f32 out[3]) {
    s32 j;
    v3_set(out, 0, 0, 0);
    for (j = 0; j < 3; j++) v3_addScaled(out, v->ax[j], v3_dot(in, v->ax[j]) / v->inertia[j]);
}

/* impulse j along dir at offset r from the centre of mass */
static void applyImpulse(NBVehicle *v, const f32 r[3], const f32 dir[3], f32 j) {
    f32 rx[3], dw[3];
    v3_addScaled(v->v, dir, j / v->mass);
    v3_cross(rx, r, dir);
    applyInvI(v, rx, dw);
    v3_addScaled(v->w, dw, j);
}

static f32 effMass(NBVehicle *v, const f32 r[3], const f32 dir[3]) {
    f32 rx[3], ir[3], c[3];
    v3_cross(rx, r, dir);
    applyInvI(v, rx, ir);
    v3_cross(c, ir, r);
    return 1.0f / (1.0f / v->mass + v3_dot(dir, c));
}

static s32 hasFuel(NBVehicle *v) {
    return v->fuel > 0.0f;
}

static void simulate(NBVehicle *v, f32 h, s32 doHull) {
    f32 M = v->mass;
    f32 up[3], fwd[3], tmp[3];
    s32 i, contacts = 0, wheelContacts = 0, nW = v->numWheels;
    f32 drive = 0.0f;
    s32 fuelOk = hasFuel(v);
    f32 boost = ((nbIn.held & BTN_Z) && v->driving && fuelOk) ? 1.0f : 0.0f;

    v3_set(sF, 0, -GRAVITY * M, 0);
    v3_set(sT, 0, 0, 0);
    v3_copy(up, v->ax[1]);
    v3_copy(fwd, v->ax[2]);

    if (v->enginePower > 0 && fuelOk && nW > 0)
        drive = v->throttle * v->enginePower * DRIVE_K / nW;

    /* wheels */
    for (i = 0; i < nW; i++) {
        NBWheel *w = &v->wheels[i];
        f32 mount[3], start[3], end[3], n[3], vp[3], f[3], s[3];
        /* travel T: 60% above the mount, 40% droop below it. At rest the spring
         * is compressed by `droop`, which puts the wheel centre on its mount. */
        f32 T = w->ski ? 10.0f : w->radius * 0.6f;
        f32 above = T * 0.6f, droop = T * 0.4f;
        f32 len = above + w->radius + droop, comp, fs, vs, vf, vsd, flat, flong, maxF;
        f32 k = (M * GRAVITY / (nW > 0 ? nW : 1)) / droop;
        f32 c = 1.3f * nb_sqrtf(k * M / (nW > 0 ? nW : 1));

        vehicle_localToWorld(v, w->pos, mount);
        v3_copy(start, mount);
        v3_addScaled(start, up, above);
        v3_copy(end, start);
        v3_addScaled(end, up, -len);
        w->contact = 0;
        if (func_80320B98(start, end, n, NB_FLOOR_FLAGS) == NULL) {
            w->compression = nb_lerpf(w->compression, -droop, 0.3f);
            if (!w->ski) w->spin += v->throttle * 12.0f * h;
            continue;
        }
        v3_sub(tmp, end, start);
        comp = len - v3_len(tmp);
        if (comp < 0) comp = 0;
        w->compression = comp - droop;  /* >0: wheel pushed up above its mount */
        w->contact = 1;
        wheelContacts++;

        pointVel(v, end, vp);
        vs = v3_dot(vp, up);
        fs = k * comp - c * vs;
        if (fs < 0) fs = 0;
        maxF = M * GRAVITY * 4.0f / (nW > 0 ? nW : 1);
        if (fs > maxF) fs = maxF;

        /* tyre frame on the ground plane */
        v3_copy(f, fwd);
        if (w->steer) {
            f32 ang = -v->steer * 32.0f;
            f32 rr = DEG2RAD(ang), cs = cosf(rr), sn = sinf(rr);
            f32 left[3];
            v3_copy(left, v->ax[0]);
            v3_scale(f, fwd, cs);
            v3_addScaled(f, left, sn);
        }
        v3_addScaled(f, n, -v3_dot(f, n));
        v3_normalize(f);
        v3_cross(s, n, f);
        v3_normalize(s);

        vf = v3_dot(vp, f);
        vsd = v3_dot(vp, s);

        flat = -vsd * (M / nW) / h * (w->ski ? 0.25f : 0.5f);
        maxF = fs * (w->ski ? 0.6f : 1.2f) * w->grip;
        flat = nb_clampf(flat, -maxF, maxF);

        if (w->ski) {
            flong = -vf * (M / nW) * 0.08f;
        } else {
            flong = drive - vf * (M / nW) * 0.35f;
            if (v->driving && (nbIn.held & BTN_B) && vf > 60.0f) {
                flong -= nb_clampf(vf * (M / nW) / h, 0, M * 1600.0f / nW);
            }
            if (!v->driving || v->throttle == 0.0f) {
                /* parking brake when nobody's driving / idle and slow */
                if (nb_absf(vf) < 120.0f) flong -= vf * (M / nW) / h * 0.3f;
            }
        }
        maxF = fs * 1.0f * w->grip;
        flong = nb_clampf(flong, -maxF, maxF);

        {
            f32 force[3], at[3], lever;
            v3_scale(force, n, fs);
            applyForce(v, force, end);
            v3_scale(force, s, flat);
            v3_addScaled(force, f, flong);
            /* apply tyre forces closer to the centre of mass height to limit roll-overs */
            v3_copy(at, end);
            v3_sub(tmp, v->x, end);
            lever = v3_dot(tmp, up);
            v3_addScaled(at, up, lever * 0.65f);
            applyForce(v, force, at);
        }
        if (!w->ski) w->spin += vf / w->radius * h;
    }

    /* aero */
    for (i = 0; i < v->numWings; i++) {
        NBAero *a = &v->wings[i];
        f32 pw[3], vp[3], ax[3], u, vn, lift;
        vehicle_localToWorld(v, a->pos, pw);
        localDirToWorld(v, a->axis, ax);
        pointVel(v, pw, vp);
        u = v3_dot(vp, fwd);
        vn = v3_dot(vp, ax);
        lift = -vn * nb_absf(u) * a->area * LIFT_K;
        if (u > 0) lift += u * u * a->area * CAMBER_K;
        lift = nb_clampf(lift, -M * GRAVITY * 3.0f, M * GRAVITY * 3.0f);
        v3_scale(tmp, ax, lift);
        applyForce(v, tmp, pw);
        /* induced drag */
        v3_scale(tmp, fwd, -nb_absf(lift) * 0.04f);
        applyForce(v, tmp, pw);
    }
    /* tail stability: winged craft turn their nose into the airflow */
    if (v->numWings > 0 || v->numFins > 0) {
        f32 sp = v3_len(v->v);
        if (sp > 150.0f) {
            f32 vd[3], c[3], k;
            v3_scale(vd, v->v, 1.0f / sp);
            if (v3_dot(vd, fwd) > 0.2f) {
                v3_cross(c, fwd, vd);
                k = nb_clampf(sp / 900.0f, 0.0f, 1.6f) * (f32)(v->numWings + v->numFins) * 2.5f;
                v3_addScaled(sT, v->ax[0], v3_dot(c, v->ax[0]) * v->inertia[0] * k);
                v3_addScaled(sT, up, v3_dot(c, up) * v->inertia[1] * k);
            }
        }
    }
    for (i = 0; i < v->numFins; i++) {
        NBAero *a = &v->fins[i];
        f32 pw[3], vp[3], ax[3], u, vn, f;
        vehicle_localToWorld(v, a->pos, pw);
        localDirToWorld(v, a->axis, ax);
        pointVel(v, pw, vp);
        u = v3_dot(vp, fwd);
        vn = v3_dot(vp, ax);
        f = -vn * nb_absf(u) * a->area * FIN_K;
        f = nb_clampf(f, -M * GRAVITY, M * GRAVITY);
        v3_scale(tmp, ax, f);
        applyForce(v, tmp, pw);
    }

    /* thrusters */
    for (i = 0; i < v->numThrust; i++) {
        NBThruster *t = &v->thrust[i];
        f32 pw[3], dir[3], amt;
        if (!fuelOk) break;
        amt = t->jet ? boost : v->throttle;
        if (amt == 0.0f) continue;
        if (!t->jet && amt < 0) amt *= 0.5f;
        if (!t->jet) amt *= nb_clampf(1.0f - v3_dot(v->v, fwd) / 2600.0f, 0.25f, 1.0f);
        vehicle_localToWorld(v, t->pos, pw);
        localDirToWorld(v, t->dir, dir);
        v3_scale(tmp, dir, t->force * amt);
        applyForce(v, tmp, pw);
    }

    /* balloons: lift fades out with height above the ground */
    if (v->numBalloons > 0) {
        f32 start[3], end[3], n[3], hgt = 2000.0f, fade;
        v3_copy(start, v->x);
        v3_copy(end, v->x);
        end[1] -= 2000.0f;
        if (func_80320B98(start, end, n, NB_FLOOR_FLAGS) != NULL) hgt = v->x[1] - end[1];
        if (v->inWater && v->waterY > end[1]) hgt = v->x[1] - v->waterY;
        fade = nb_clampf(1.0f - (hgt - 350.0f) / 700.0f, 0.0f, 1.0f);
        if (v->driving && (nbIn.held & BTN_B)) fade *= 0.4f;    /* vent */
        if (v->driving && (nbIn.held & BTN_A) && v->numProps == 0) fade = 1.0f;
        /* balloons are big and draggy */
        {
            f32 sp = v3_len(v->v);
            v3_scale(tmp, v->v, -sp * 0.0045f * v->numBalloons * GRAVITY / 100.0f);
            v3_add(sF, sF, tmp);
        }
        for (i = 0; i < v->numBalloons; i++) {
            f32 pw[3];
            vehicle_localToWorld(v, v->balloons[i], pw);
            v3_set(tmp, 0, BALLOON_LIFT * GRAVITY * fade, 0);
            applyForce(v, tmp, pw);
        }
    }

    /* water */
    if (v->inWater) {
        s32 submerged = 0;
        for (i = 0; i < v->numFloat; i++) {
            f32 pw[3], depth;
            vehicle_localToWorld(v, v->floaters[i], pw);
            depth = v->waterY - pw[1];
            if (depth > -18.0f) {
                f32 amt = nb_clampf((depth + 18.0f) / 36.0f, 0.0f, 1.3f);
                v3_set(tmp, 0, FLOAT_LIFT * GRAVITY * amt, 0);
                applyForce(v, tmp, pw);
                submerged++;
            }
        }
        if (v->x[1] < v->waterY + 10.0f || submerged) {
            /* water drag and a little natural buoyancy */
            v3_scale(tmp, v->v, -M * 1.3f);
            v3_add(sF, sF, tmp);
            if (v->x[1] < v->waterY) sF[1] += M * GRAVITY * 0.55f;
            v3_scale(v->w, v->w, 1.0f / (1.0f + h * 2.5f));
        }
    }

    /* air drag */
    {
        f32 sp = v3_len(v->v);
        v3_scale(tmp, v->v, -(DRAG_Q * sp + DRAG_L) * M);
        v3_add(sF, sF, tmp);
    }

    /* steering assist (yaw), air control, self-levelling */
    if (v->driving) {
        f32 fs = v3_dot(v->v, fwd);
        f32 airborne = (wheelContacts == 0 && !v->onGround);
        f32 yawRate = v3_dot(v->w, up);
        if (!airborne) {
            f32 target = -v->steer * nb_clampf(fs / 450.0f, -1.0f, 1.0f) * 1.6f;
            f32 tq = (target - yawRate) * v->inertia[1] * 3.0f;
            v3_addScaled(sT, up, tq);
            /* take-off rotation: winged craft lift their nose ~6 degrees at speed */
            if (v->numWings > 0 && fs > 550.0f && v->throttle > 0.5f) {
                f32 want = 0.10f - nbIn.sy * 0.08f;
                v3_addScaled(sT, v->ax[0], -(want - fwd[1]) * v->inertia[0] * 10.0f);
            }
        } else {
            /* rate control in the air: the stick asks for a pitch/roll rate and
             * letting go damps the rotation. Wings and fins give more authority. */
            f32 aero = (f32)(v->numWings * 2 + v->numFins + (v->numBalloons ? 2 : 0));
            f32 auth = nb_clampf(0.8f + aero * 0.9f, 0.8f, 6.0f);
            f32 pr = v3_dot(v->w, v->ax[0]), rr = v3_dot(v->w, fwd);
            f32 tp = nbIn.sy * 1.5f, tr = nbIn.sx * 2.0f, ty = -nbIn.sx * 0.9f;
            v3_addScaled(sT, v->ax[0], (tp - pr) * v->inertia[0] * auth);
            v3_addScaled(sT, fwd, (tr - rr) * v->inertia[2] * auth);
            if (aero > 0) {
                v3_addScaled(sT, up, (ty - yawRate) * v->inertia[1] * auth * 0.5f);
                if (nbIn.sx == 0.0f) {
                    /* wings level themselves when you let go */
                    v3_addScaled(sT, fwd, -v->ax[0][1] * v->inertia[2] * 4.0f);
                }
            }
        }
        if (v->numGyros > 0) {
            /* gyroscopes pull the vehicle back to level */
            f32 corr[3], wup[3] = {0, 1, 0}, k = nb_clampf(v->numGyros * 9.0f, 0, 30.0f);
            v3_cross(corr, up, wup);
            v3_addScaled(sT, corr, v->inertia[0] * k);
            v3_addScaled(sT, v->ax[0], -v3_dot(v->w, v->ax[0]) * v->inertia[0] * 2.0f);
            v3_addScaled(sT, fwd, -v3_dot(v->w, fwd) * v->inertia[2] * 2.0f);
        }
        if (v->numBalloons || v->inWater) {
            /* gentle self-righting so blimps and boats stay upright */
            f32 corr[3], wup[3] = {0, 1, 0};
            v3_cross(corr, up, wup);
            v3_addScaled(sT, corr, v->inertia[0] * 6.0f);
        }
    }

    /* integrate velocities */
    {
        f32 a[3], alpha[3] = {0, 0, 0}, damp;
        s32 j;
        v3_scale(a, sF, 1.0f / M);
        v3_addScaled(v->v, a, h);
        for (j = 0; j < 3; j++) {
            f32 tb = v3_dot(sT, v->ax[j]) / v->inertia[j];
            v3_addScaled(alpha, v->ax[j], tb);
        }
        v3_addScaled(v->w, alpha, h);
        damp = wheelContacts ? 3.0f : 1.2f;
        v3_scale(v->w, v->w, 1.0f / (1.0f + h * damp));
    }

    /* hull contacts: impulses against floors and walls */
    if (doHull) {
        f32 push[3] = {0, 0, 0};
        for (i = 0; i < v->numHull; i++) {
            f32 pw[3], end[3], n[3], vp[3], vt[3], r[3], pen, vn, jn, jt, sp;
            vehicle_localToWorld(v, v->hull[i], pw);
            v3_copy(end, pw);
            if (func_80320B98(v->x, end, n, NB_FLOOR_FLAGS) == NULL) continue;
            v3_sub(tmp, end, pw);
            pen = v3_dot(tmp, n);
            if (pen <= 0) continue;
            contacts++;
            v3_sub(r, pw, v->x);
            pointVel(v, pw, vp);
            vn = v3_dot(vp, n);
            if (vn < -700.0f) {
                damageNear(v, v->hull[i], (s32)((-vn - 700.0f) * 0.08f) + 5);
                if (vn < -900.0f) gcsfx_playWithPitch(SFX_99_METALLIC_THUD, 1.0f, 26000);
            }
            if (vn < 0) {
                jn = -1.15f * vn * effMass(v, r, n);
                applyImpulse(v, r, n, jn);
                /* Coulomb friction */
                pointVel(v, pw, vp);
                vn = v3_dot(vp, n);
                v3_copy(vt, vp);
                v3_addScaled(vt, n, -vn);
                sp = v3_len(vt);
                if (sp > 1.0f) {
                    v3_scale(vt, vt, 1.0f / sp);
                    jt = sp * effMass(v, r, vt);
                    if (jt > jn * 0.45f) jt = jn * 0.45f;
                    applyImpulse(v, r, vt, -jt);
                }
            }
            /* positional correction (keep the largest push per axis) */
            {
                s32 j;
                f32 c = (pen - 0.5f) * 0.5f;
                if (c > 0)
                    for (j = 0; j < 3; j++) {
                        f32 d = n[j] * c;
                        if ((d > 0 && d > push[j]) || (d < 0 && d < push[j])) push[j] = d;
                    }
            }
        }
        v3_add(v->x, v->x, push);
    }

    v->onGround = (wheelContacts > 0) || (contacts > 0);
    v->dbgWheels = wheelContacts;
    if (doHull) v->dbgHull = contacts;

    /* integrate positions */
    {
        s32 j;
        f32 sp = v3_len(v->w);
        if (sp > 12.0f) v3_scale(v->w, v->w, 12.0f / sp);
        v3_addScaled(v->x, v->v, h);
        for (j = 0; j < 3; j++) {
            f32 dw[3];
            v3_cross(dw, v->w, v->ax[j]);
            v3_addScaled(v->ax[j], dw, h);
        }
        v3_normalize(v->ax[2]);
        v3_addScaled(v->ax[1], v->ax[2], -v3_dot(v->ax[1], v->ax[2]));
        v3_normalize(v->ax[1]);
        v3_cross(v->ax[0], v->ax[1], v->ax[2]);
    }
}

static void recompileInPlace(NBVehicle *v) {
    f32 origin[3], oldcm[3], cmw[3];
    v3_copy(oldcm, v->cm);
    localDirToWorld(v, oldcm, cmw);
    v3_sub(origin, v->x, cmw);
    if (vehicle_compile(v, &nbBlueprint) <= 0) {
        vehicle_despawn(v);
        return;
    }
    localDirToWorld(v, v->cm, cmw);
    v3_add(v->x, origin, cmw);
    if (v->fuel > v->fuelMax) v->fuel = v->fuelMax;
    v->needsRecompile = FALSE;
}

static void updateWater(NBVehicle *v) {
    f32 start[3], end[3], n[3];
    BKCollisionTriangle *tri;
    v3_copy(start, v->x);
    start[1] += 600.0f;
    v3_copy(end, v->x);
    end[1] -= 400.0f;
    tri = mapModel_intersectLine(start, end, n, NB_WATER_FLAGS);
    if (tri != NULL && (tri->flags & COLL_FLAG_WATER)) {
        if (!v->inWater && v->x[1] < end[1] + 30.0f && v->v[1] < -300.0f)
            gcsfx_playWithPitch(SFX_F_SMALL_WATER_SPLASH, 1.0f, 30000);
        v->waterY = end[1];
        v->inWater = TRUE;
    } else {
        v->inWater = FALSE;
        v->waterY = -100000.0f;
    }
}

static void updateSound(NBVehicle *v, f32 dt) {
    f32 pitch, vol;
    if (!v->driving || (v->enginePower <= 0 && v->numThrust == 0)) {
        vehicle_soundStop(v);
        return;
    }
    if (!v->engineSfx) {
        v->engineSfx = sfxsource_createSfxsourceAndReturnIndex();
        if (!v->engineSfx) return;
        sfxsource_set_fade_distances(v->engineSfx, 600.0f, 2500.0f);
        sfxsource_set_position(v->engineSfx, v->x);
        sfxsource_playSfxAtVolume(v->engineSfx, 0.8f);
        sfxsource_setSfxId(v->engineSfx, v->numProps && v->enginePower <= 0 ? SFX_104_PROPELLER_NOISE : SFX_9F_GENERATOR_RUNNING);
        sfxSource_setunk43_7ByIndex(v->engineSfx, 3);
        sfxsource_setSampleRate(v->engineSfx, 12000);
        sfxSource_func_8030E2C4(v->engineSfx);
    }
    pitch = 0.7f + nb_absf(v->throttle) * 0.25f + nb_clampf(v->speed / 2400.0f, 0.0f, 0.6f);
    vol = hasFuel(v) ? 11000.0f + nb_absf(v->throttle) * 12000.0f : 0.0f;
    sfxsource_set_position(v->engineSfx, v->x);
    sfxsource_playSfxAtVolume(v->engineSfx, pitch);
    sfxsource_setSampleRate(v->engineSfx, (s32)vol);
}

void vehicle_step(NBVehicle *v, f32 dt) {
    s32 n, i;
    f32 h;

    if (!v->active) return;
    if (v->needsRecompile) {
        recompileInPlace(v);
        if (!v->active) return;
        if (v->broken[v->seatPart] && nbMode == MODE_DRIVE) nb_exitVehicle();
    }
    if (dt <= 0) return;

    updateWater(v);

    n = (s32)(dt / SUBSTEP + 0.999f);
    if (n < 1) n = 1;
    if (n > 4) n = 4;
    h = dt / n;
    for (i = 0; i < n; i++) simulate(v, h, TRUE);

    v->speed = v3_len(v->v);
    v->spinAngle += (v->throttle != 0.0f && hasFuel(v) ? 25.0f : 4.0f) * dt;
    if (v->spinAngle > 2 * NB_PI) v->spinAngle -= 2 * NB_PI;

    /* fuel */
    if (v->driving && v->fuelMax > 0) {
        f32 use = nb_absf(v->throttle) * (v->enginePower * 1.2f + v->numProps * 0.8f);
        if (nbIn.held & BTN_Z) use += v->numJets * 9.0f;
        if (v->fuel > 0 && v->fuel - use * dt <= 0) nb_message("OUT OF FUEL! D-DOWN: GARAGE", 2.5f);
        v->fuel -= use * dt;
        if (v->fuel < 0) v->fuel = 0;
    }
    if (v->cannonCooldown > 0) v->cannonCooldown -= dt;
    if (v->springCooldown > 0) v->springCooldown -= dt;
    if (v->hornCooldown > 0) v->hornCooldown -= dt;

    /* upside down / lost / fallen out of the world */
    /* upside down, or wedged on its side/nose and not going anywhere */
    if ((v->ax[1][1] < 0.2f && v->speed < 200.0f) || (v->ax[1][1] < 0.6f && v->speed < 60.0f && v->onGround))
        v->upsideTime += dt;
    else
        v->upsideTime = 0;
    if (v->onGround || v->inWater) {
        v->airTime = 0;
        v->safeTimer += dt;
        if (v->safeTimer > 1.0f && v->ax[1][1] > 0.8f && !v->inWater) {
            v->safeTimer = 0;
            v3_copy(v->safePos, v->x);
            v->safeYaw = vehicle_yaw(v);
        }
    } else {
        v->airTime += dt;
    }
    if ((v->airTime > 6.0f && v->numWings == 0 && v->numBalloons == 0) || v->x[1] < v->safePos[1] - 4000.0f) {
        v3_copy(v->x, v->safePos);
        v->x[1] += 60.0f;
        setYawAxes(v, v->safeYaw);
        v3_set(v->v, 0, 0, 0);
        v3_set(v->w, 0, 0, 0);
        v->airTime = 0;
        nb_message("BACK ON TRACK!", 1.5f);
    }

    updateSound(v, dt);
}

/* driver input -> throttle/steer + gadgets */
void vehicle_controls(NBVehicle *v, f32 dt) {
    f32 fs = v3_dot(v->v, v->ax[2]);
    f32 targetThrottle = 0.0f;

    if (nbIn.held & BTN_A) targetThrottle = 1.0f;
    else if ((nbIn.held & BTN_B) && fs < 80.0f) targetThrottle = -0.45f;
    v->throttle = nb_lerpf(v->throttle, targetThrottle, nb_clampf(dt * 6.0f, 0, 1));
    if (nb_absf(v->throttle) < 0.02f && targetThrottle == 0.0f) v->throttle = 0.0f;
    v->steer = nb_lerpf(v->steer, nbIn.sx, nb_clampf(dt * 8.0f, 0, 1));

    if ((nbIn.pressed & BTN_DU) || v->upsideTime > 2.5f) {
        if (v->ax[1][1] < 0.5f || (nbIn.pressed & BTN_DU)) vehicle_flipUpright(v);
        v->upsideTime = 0;
    }

    /* C-Up: shock springs */
    if ((nbIn.pressed & BTN_CU) && v->numSprings > 0 && v->springCooldown <= 0 && v->onGround) {
        f32 imp = nb_clampf(900.0f * v->numSprings * 30.0f / v->mass, 500.0f, 1500.0f);
        v->v[1] += imp;
        v->springCooldown = 0.6f;
        gcsfx_playWithPitch(SFX_E_SHOCKSPRING_BOING, 1.0f, 30000);
    }

    /* R: horn + egg cannons */
    if (nbIn.pressed & BTN_R) {
        if (v->numHorns > 0 && v->hornCooldown <= 0) {
            gcsfx_playWithPitch(SFX_17A_SHIPHORN, 1.0f, 28000);
            v->hornCooldown = 0.8f;
        }
        if (v->numCannonPos > 0 && v->cannonCooldown <= 0) {
            if (item_empty(ITEM_D_EGGS)) {
                nb_message("OUT OF EGGS!", 1.2f);
                gcsfx_playWithPitch(SFX_90_SWITCH_PRESS, 0.7f, 20000);
            } else {
                /* the egg projectile spawns from Banjo's position and facing,
                 * so borrow them for a moment */
                f32 saved[3], muzzle[3];
                s32 i;
                playerPosition_get(saved);
                for (i = 0; i < v->numCannonPos && !item_empty(ITEM_D_EGGS); i++) {
                    vehicle_localToWorld(v, v->cannons[i], muzzle);
                    muzzle[1] -= 80.0f;
                    playerPosition_set(muzzle);
                    yaw_set(vehicle_yaw(v) + v->cannons[i][3]);
                    commonParticle_new(COMMON_PARTICLE_1_EGG_HEAD, 1);
                    item_dec(ITEM_D_EGGS);
                }
                playerPosition_set(saved);
                gcsfx_playWithPitch(SFX_3_DULL_CANNON_SHOT, 1.0f, 28000);
                v->cannonCooldown = 0.35f;
            }
        }
    }
}
