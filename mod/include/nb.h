#ifndef NB_H
#define NB_H
/*
 * Nuts & Bolts 64 - a Banjo-Kazooie: Nuts & Bolts style vehicle builder for
 * Banjo-Kazooie (N64). Shared types and prototypes.
 */
#include <ultra64.h>
#include "bk.h"
#include "nb_textures.h"

#define NB_VERSION 1

/* Banjo-Kazooie (NTSC) renders into a 292x216 framebuffer */
#define SCREEN_W 292
#define SCREEN_H 216

#ifndef NULL
#define NULL ((void *)0)
#endif
#define TRUE 1
#define FALSE 0

/* ---- build grid ---- */
#define CELL        40.0f   /* world units per grid cell (Banjo is ~3 cells tall) */
#define GRID_HALF   7       /* x and z run -7..7 */
#define GRID_H      8       /* y runs 0..7 */
#define GRID_N      15
#define MAX_PARTS   160
#define NUM_SLOTS   3

/* ---- part categories / kinds ---- */
enum nb_category_e {
    CAT_BODY, CAT_MOVE, CAT_POWER, CAT_FLIGHT, CAT_GADGET, CAT_DECOR, CAT_COUNT
};

enum nb_kind_e {
    KIND_BLOCK, KIND_WHEEL, KIND_ENGINE, KIND_FUEL, KIND_SEAT, KIND_PROP,
    KIND_JET, KIND_WING, KIND_FIN, KIND_FLOATER, KIND_BALLOON, KIND_SPRING,
    KIND_CANNON, KIND_HORN, KIND_LIGHT, KIND_DECOR, KIND_SKI, KIND_TREAD
};

#define PF_PAINT  0x01  /* tinted with the part's paint colour */
#define PF_SPIN_X 0x02  /* spin mesh rotates around local X (wheels) */
#define PF_SPIN_Z 0x04  /* spin mesh rotates around local Z (propellers) */

typedef struct {
    const char *name;
    u8  category;
    u8  kind;
    u8  sx, sy, sz;      /* footprint in cells, before rotation */
    u8  flags;
    u8  unlock;          /* Jiggies needed (0 = always available) */
    u8  defaultColor;
    f32 mass;
    s16 hp;
    f32 value;           /* kind specific: wheel radius, engine power, fuel, thrust, lift area... */
} NBPartDef;

extern const NBPartDef nbPartDefs[];
extern const s32 nbPartCount;
extern const char *nbCategoryNames[CAT_COUNT];

#define NB_NUM_COLORS 10
extern const u8 nbPalette[NB_NUM_COLORS][3];
extern const char *nbColorNames[NB_NUM_COLORS];

/* ---- blueprint ---- */
typedef struct {
    u8 type;
    s8 x, y, z;          /* min corner cell */
    u8 rot;              /* 0..3, quarter turns of yaw */
    u8 color;
} NBPart;

typedef struct {
    u16 count;
    u16 pad;
    NBPart parts[MAX_PARTS];
} NBBlueprint;

void bp_clear(NBBlueprint *bp);
void bp_partExtent(const NBPart *p, s32 *sx, s32 *sy, s32 *sz);
s32  bp_findAt(const NBBlueprint *bp, s32 x, s32 y, s32 z);
s32  bp_canPlace(const NBBlueprint *bp, s32 type, s32 x, s32 y, s32 z, s32 rot);
s32  bp_add(NBBlueprint *bp, s32 type, s32 x, s32 y, s32 z, s32 rot, s32 color);
void bp_remove(NBBlueprint *bp, s32 index);
void bp_partCenter(const NBPart *p, f32 out[3]);
s32  bp_countKind(const NBBlueprint *bp, s32 kind);

#define NB_NUM_PRESETS 6
extern const char *nbPresetNames[NB_NUM_PRESETS];
void preset_build(s32 index, NBBlueprint *bp);

/* ---- meshes ---- */
typedef struct {
    Gfx *body;           /* static geometry (may be NULL) */
    Gfx *spin;           /* rotating geometry (may be NULL) */
    f32 spinPivot[3];    /* pivot of the spinning geometry, mesh space */
} NBMesh;

extern NBMesh nbMeshes[];
extern Gfx *nbGridDL;
extern Gfx *nbCursorDL;
extern Gfx *nbPadDL;
extern Gfx *nbShadowDL;
void mesh_buildAll(void);

/* ---- math ---- */
#define NB_PI 3.14159265f
#define DEG2RAD(d) ((d) * (NB_PI / 180.0f))
#define RAD2DEG(r) ((r) * (180.0f / NB_PI))
f32  nb_sqrtf(f32 x);
f32  nb_atan2f(f32 y, f32 x);
f32  nb_absf(f32 x);
f32  nb_clampf(f32 x, f32 lo, f32 hi);
f32  nb_lerpf(f32 a, f32 b, f32 t);
f32  nb_wrapDeg(f32 d);
void v3_set(f32 d[3], f32 x, f32 y, f32 z);
void v3_copy(f32 d[3], const f32 s[3]);
void v3_add(f32 d[3], const f32 a[3], const f32 b[3]);
void v3_sub(f32 d[3], const f32 a[3], const f32 b[3]);
void v3_scale(f32 d[3], const f32 a[3], f32 s);
void v3_addScaled(f32 d[3], const f32 a[3], f32 s);
f32  v3_dot(const f32 a[3], const f32 b[3]);
void v3_cross(f32 d[3], const f32 a[3], const f32 b[3]);
f32  v3_len(const f32 a[3]);
f32  v3_normalize(f32 a[3]);
void v3_rotateYaw(f32 d[3], const f32 s[3], f32 yawDeg);
void mtx_fromAxes(Mtx *m, f32 ax[3][3], const f32 t[3]);
void nb_itoa(char *dst, s32 v);
char *nb_strcat(char *dst, const char *src);
char *nb_strcpy(char *dst, const char *src);
void nb_memset(void *dst, s32 v, u32 n);
void nb_memcpy(void *dst, const void *src, u32 n);

/* ---- input ---- */
#define BTN_A      0x8000
#define BTN_B      0x4000
#define BTN_Z      0x2000
#define BTN_START  0x1000
#define BTN_DU     0x0800
#define BTN_DD     0x0400
#define BTN_DL     0x0200
#define BTN_DR     0x0100
#define BTN_L      0x0020
#define BTN_R      0x0010
#define BTN_CU     0x0008
#define BTN_CD     0x0004
#define BTN_CL     0x0002
#define BTN_CR     0x0001

typedef struct {
    u16 held, pressed, released;
    f32 sx, sy;          /* -1..1 with deadzone */
} NBInput;
extern NBInput nbIn;
void input_update(void);

/* ---- vehicle ---- */
#define MAX_WHEELS   12
#define MAX_THRUST   12
#define MAX_AERO     24
#define MAX_HULL     26
#define MAX_FLOAT    16
#define MAX_DEBRIS   16

typedef struct {
    f32 pos[3];          /* local, relative to centre of mass */
    f32 radius;
    f32 grip;
    f32 spin;            /* radians */
    f32 compression;
    u8  steer;
    u8  contact;
    u8  part;            /* index into blueprint */
    u8  ski;
} NBWheel;

typedef struct {
    f32 pos[3];
    f32 dir[3];
    f32 force;
    u8  jet;
    u8  part;
} NBThruster;

typedef struct {
    f32 pos[3];
    f32 axis[3];         /* lift axis, local */
    f32 area;
} NBAero;

typedef struct {
    f32 pos[3];
    f32 vel[3];
    f32 ax[3][3];
    f32 spin[3];
    f32 timer;
    u8  type, color, rot, active;
} NBDebris;

typedef struct {
    /* compiled from the blueprint */
    s32 valid;
    f32 mass;
    f32 cm[3];               /* blueprint-space centre of mass */
    f32 inertia[3];
    f32 seat[3];             /* seat top, relative to cm */
    s32 seatPart;
    s32 seatRot;
    f32 enginePower;
    f32 fuelMax;
    f32 halfExtent[3];
    s32 numWheels;  NBWheel wheels[MAX_WHEELS];
    s32 numThrust;  NBThruster thrust[MAX_THRUST];
    s32 numWings;   NBAero wings[MAX_AERO];
    s32 numFins;    NBAero fins[MAX_AERO];
    s32 numFloat;   f32 floaters[MAX_FLOAT][3];
    s32 numHull;    f32 hull[MAX_HULL][3];
    s32 numBalloons;f32 balloons[MAX_FLOAT][3];
    s32 numSprings, numCannons, numHorns, numJets, numProps;
    f32 cannons[8][4];       /* pos xyz + yaw offset */
    s32 numCannonPos;
    s16 hp[MAX_PARTS];
    u8  broken[MAX_PARTS];

    /* rigid body state */
    s32 active;              /* exists in the world */
    s32 driving;             /* Banjo is in the seat */
    f32 x[3];
    f32 v[3];
    f32 ax[3][3];            /* body axes in world space: left, up, forward */
    f32 w[3];
    f32 fuel;
    f32 throttle, steer;
    f32 spinAngle;           /* propellers */
    f32 airTime, upsideTime, groundTime;
    s32 onGround;
    f32 waterY;
    s32 inWater;
    f32 safePos[3];
    f32 safeYaw;
    f32 safeTimer;
    f32 cannonCooldown, springCooldown, hornCooldown;
    f32 speed;
    s32 needsRecompile;
    u8  engineSfx;
    s32 dbgWheels, dbgHull;
} NBVehicle;

extern NBVehicle nbVeh;
extern NBBlueprint nbBlueprint;
extern NBBlueprint nbSlots[NUM_SLOTS];
extern NBDebris nbDebris[MAX_DEBRIS];

s32  vehicle_compile(NBVehicle *v, const NBBlueprint *bp);
void vehicle_spawn(NBVehicle *v, const f32 floorPos[3], f32 yawDeg);
void vehicle_despawn(NBVehicle *v);
void vehicle_step(NBVehicle *v, f32 dt);
void vehicle_controls(NBVehicle *v, f32 dt);
void vehicle_seatWorld(NBVehicle *v, f32 out[3]);
void vehicle_localToWorld(NBVehicle *v, const f32 local[3], f32 out[3]);
f32  vehicle_yaw(NBVehicle *v);
void vehicle_flipUpright(NBVehicle *v);
void vehicle_soundStop(NBVehicle *v);
void debris_update(f32 dt);

/* ---- garage ---- */
typedef struct {
    s32 active;
    f32 origin[3];           /* world position of the grid's floor centre */
    f32 yaw;                 /* grid forward yaw (deg) */
    s32 cx, cy, cz;          /* cursor cell */
    s32 type;                /* selected part */
    s32 rot;
    s32 color;
    f32 camYaw;              /* relative to grid yaw */
    f32 camDist;
    f32 camTarget[3];
    f32 repeat;
    s32 lastDir;
    f32 holdB;
    s32 menu;                /* 0 = closed, otherwise open */
    s32 menuSel;
    s32 menuBlueprint;       /* 0..2 slots, 3.. presets */
    s32 menuSaveSlot;
    f32 blink;
} NBGarage;

extern NBGarage nbGarage;
void garage_enter(const f32 floorPos[3], f32 yawDeg);
void garage_update(f32 dt);
void garage_camera(f32 dt);
void garage_cellToWorld(f32 cx, f32 cy, f32 cz, f32 out[3]);

/* ---- modes ---- */
enum nb_mode_e { MODE_NORMAL, MODE_GARAGE, MODE_DRIVE };
extern s32 nbMode;
void nb_setMode(s32 mode);
void nb_message(const char *msg, f32 seconds);
s32  nb_partUnlocked(s32 type);
void nb_exitVehicle(void);

/* ---- camera ---- */
extern f32 nbCamPos[3], nbCamRot[3];
extern s32 nbCamActive;
void cam_lookAt(f32 eye[3], f32 target[3]);
void cam_driveUpdate(f32 dt);

/* ---- rendering ---- */
void render_world(Gfx **gfx, Mtx **mtx, Vtx **vtx);
void render_hud(Gfx **gfx, Mtx **mtx, Vtx **vtx);

/* ---- hud text ---- */
void hud_queueText(s32 x, s32 y, const char *str, u8 r, u8 g, u8 b);
void hud_flushText(void);
void hud_rect(s32 x, s32 y, s32 w, s32 h, u8 r, u8 g, u8 b, u8 a);
extern f32 nbMessageTimer;
extern char nbMessage[40];

#endif
