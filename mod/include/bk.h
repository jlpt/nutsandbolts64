#ifndef NB_BK_H
#define NB_BK_H
/*
 * Declarations for the Banjo-Kazooie (US v1.0) engine functions the mod
 * calls. Addresses are resolved at link time from the decomp's ELF (see
 * tools/gen_syms.py), so names here must match the decomp's symbols.
 */
#include <ultra64.h>

typedef struct BKCollisionTriangle {
    s16 vtx[3];
    s16 unk6;
    s32 flags;               /* 0x001E0000 = water surface */
} BKCollisionTriangle;
#define COLL_FLAG_WATER 0x001E0000

/* ---- player ---- */
void playerPosition_get(f32 pos[3]);
void playerPosition_set(f32 pos[3]);
void func_8028F85C(f32 pos[3]);                /* teleport player and resync camera/collision */
void func_8028F7C8(s32 locked);                /* player_setLocked: skip the player update */
void func_8028F784(s32 disable);               /* disable player button/stick input */
void player_setModelVisible(s32 visible);
s32  player_isStable(void);
s32  player_inWater(void);
u32  player_getTransformation(void);
s32  player_isDead(void);
s32  bs_getState(void);
void bs_setState(s32 state);
s32  bs_getIdleState(void);
f32  yaw_get(void);
void yaw_set(f32 deg);
void yaw_setIdeal(f32 deg);
void pitch_set(f32 deg);
void roll_set(f32 deg);

#define TRANSFORM_1_BANJO 1

/* ---- game state ---- */
s32  getGameMode(void);
s32  gcdialog_hasCurrentTextId(void);
s32  gsworld_getMap(void);
s32  game_is_frozen(void);
f32  time_getDelta(void);
OSContPad *joy_getInputsPrimary(void);

#define GAME_MODE_3_NORMAL 3

/* ---- camera / viewport ---- */
void viewport_setPosition_vec3f(f32 pos[3]);
void viewport_setRotation_vec3f(f32 rot[3]);   /* pitch, yaw, roll in degrees */
void viewport_getPosition_vec3f(f32 pos[3]);
void viewport_getRotation_vec3f(f32 rot[3]);
void viewport_update(void);

/* ---- text (dialog font: '!'..'^', so uppercase only) ---- */
void print_dialog(s32 x, s32 y, u8 *str);
void print_bold_spaced(s32 x, s32 y, u8 *str);
void text_setNormalTextColor(s32 r, s32 g, s32 b);
void text_setNormalTextAlpha(s32 a);

/* ---- sound ---- */
void gcsfx_playWithPitch(s32 sfx, f32 pitch, s32 volume);
u8   sfxsource_createSfxsourceAndReturnIndex(void);
void sfxsource_freeSfxsourceByIndex(u8 idx);
void sfxsource_setSfxId(u8 idx, s32 sfx);
void sfxsource_setSampleRate(u8 idx, s32 volume);    /* decomp name; really volume 0..32767 */
void sfxsource_playSfxAtVolume(u8 idx, f32 pitch);   /* decomp name; really pitch */
void sfxSource_setunk43_7ByIndex(u8 idx, s32 mode);  /* 3 = looping */
void sfxSource_func_8030E2C4(u8 idx);                /* start playback */
void sfxsource_set_fade_distances(u8 idx, f32 min, f32 max);
void sfxsource_set_position(u8 idx, f32 pos[3]);

#define SFX_3_DULL_CANNON_SHOT       0x3
#define SFX_C_TAKING_FLIGHT_LIFTOFF  0xC
#define SFX_E_SHOCKSPRING_BOING      0xE
#define SFX_F_SMALL_WATER_SPLASH     0xF
#define SFX_11_WOOD_BREAKING_1       0x11
#define SFX_1B_EXPLOSION_1           0x1B
#define SFX_20_METAL_CLANK_1         0x20
#define SFX_2D_KABOING               0x2D
#define SFX_30_MAGIC_POOF            0x30
#define SFX_82_METAL_BREAK           0x82
#define SFX_8A_ALTERNATIVE_EGG_SHOT  0x8A
#define SFX_90_SWITCH_PRESS          0x90
#define SFX_94_COGS_ROTATING         0x94
#define SFX_99_METALLIC_THUD         0x99
#define SFX_9F_GENERATOR_RUNNING     0x9F
#define SFX_A7_WOODEN_SWOSH          0xA7
#define SFX_C7_SHWOOP                0xC7
#define SFX_C9_PAUSEMENU_ENTER       0xC9
#define SFX_CC_PAUSEMENU_ENTER_SUBMENU 0xCC
#define SFX_CD_PAUSEMENU_LEAVE_SUBMENU 0xCD
#define SFX_CE_PAUSEMENU_HOIP        0xCE
#define SFX_D9_WOODEN_CRATE_BREAKING_1 0xD9
#define SFX_104_PROPELLER_NOISE      0x104
#define SFX_17A_SHIPHORN             0x17A
#define SFX_1_MUMBO_UMENAKA          0x1

/* ---- collision ---- */
BKCollisionTriangle *func_80320B98(f32 start[3], f32 end[3], f32 normal[3], u32 flags); /* all colliders */
BKCollisionTriangle *mapModel_intersectLine(f32 start[3], f32 end[3], f32 normal[3], s32 flags);
#define NB_FLOOR_FLAGS 0x1E0000
#define NB_WATER_FLAGS 0xF800FF0F

/* ---- items ---- */
s32  item_getCount(s32 item);
void item_dec(s32 item);
s32  item_empty(s32 item);
#define ITEM_D_EGGS         0xD
#define ITEM_26_JIGGY_TOTAL 0x26

/* ---- particles / projectiles ---- */
s32 commonParticle_new(s32 particle_id, s32 arg1);
#define COMMON_PARTICLE_1_EGG_HEAD 1

/* ---- libultra ---- */
f32 sinf(f32);
f32 cosf(f32);
f32 sqrtf(f32);
void bzero(void *, int);

#endif
