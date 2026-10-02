#include "nb.h"

f32 nb_sqrtf(f32 x) {
    return (x <= 0.0f) ? 0.0f : __builtin_sqrtf(x);
}

f32 nb_absf(f32 x) {
    return x < 0.0f ? -x : x;
}

f32 nb_clampf(f32 x, f32 lo, f32 hi) {
    return x < lo ? lo : (x > hi ? hi : x);
}

f32 nb_lerpf(f32 a, f32 b, f32 t) {
    return a + (b - a) * t;
}

f32 nb_wrapDeg(f32 d) {
    while (d >= 180.0f) d -= 360.0f;
    while (d < -180.0f) d += 360.0f;
    return d;
}

/* atan2 approximation, max error ~0.005 rad */
f32 nb_atan2f(f32 y, f32 x) {
    f32 ax = nb_absf(x), ay = nb_absf(y);
    f32 a, s, r;
    if (ax < 1e-9f && ay < 1e-9f) return 0.0f;
    a = (ax < ay) ? ax / ay : ay / ax;
    s = a * a;
    r = ((-0.0464964749f * s + 0.15931422f) * s - 0.327622764f) * s * a + a;
    if (ay > ax) r = 1.57079637f - r;
    if (x < 0.0f) r = NB_PI - r;
    if (y < 0.0f) r = -r;
    return r;
}

void v3_set(f32 d[3], f32 x, f32 y, f32 z) { d[0] = x; d[1] = y; d[2] = z; }
void v3_copy(f32 d[3], const f32 s[3]) { d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; }
void v3_add(f32 d[3], const f32 a[3], const f32 b[3]) { d[0] = a[0] + b[0]; d[1] = a[1] + b[1]; d[2] = a[2] + b[2]; }
void v3_sub(f32 d[3], const f32 a[3], const f32 b[3]) { d[0] = a[0] - b[0]; d[1] = a[1] - b[1]; d[2] = a[2] - b[2]; }
void v3_scale(f32 d[3], const f32 a[3], f32 s) { d[0] = a[0] * s; d[1] = a[1] * s; d[2] = a[2] * s; }
void v3_addScaled(f32 d[3], const f32 a[3], f32 s) { d[0] += a[0] * s; d[1] += a[1] * s; d[2] += a[2] * s; }
f32  v3_dot(const f32 a[3], const f32 b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

void v3_cross(f32 d[3], const f32 a[3], const f32 b[3]) {
    f32 x = a[1] * b[2] - a[2] * b[1];
    f32 y = a[2] * b[0] - a[0] * b[2];
    f32 z = a[0] * b[1] - a[1] * b[0];
    d[0] = x; d[1] = y; d[2] = z;
}

f32 v3_len(const f32 a[3]) { return nb_sqrtf(v3_dot(a, a)); }

f32 v3_normalize(f32 a[3]) {
    f32 l = v3_len(a);
    if (l > 1e-6f) v3_scale(a, a, 1.0f / l);
    return l;
}

/* Banjo-Kazooie yaw convention: yaw 0 faces +Z, +90 faces +X. */
void v3_rotateYaw(f32 d[3], const f32 s[3], f32 yawDeg) {
    f32 r = DEG2RAD(yawDeg);
    f32 c = cosf(r), sn = sinf(r);
    f32 x = s[0] * c + s[2] * sn;
    f32 z = s[2] * c - s[0] * sn;
    d[0] = x; d[1] = s[1]; d[2] = z;
}

/* Fixed-point matrix from a row-vector basis (rows = images of the local
 * X, Y, Z axes) and a translation. Same layout as guMtxF2L. */
void mtx_fromAxes(Mtx *m, f32 ax[3][3], const f32 t[3]) {
    s32 e[4][4];
    s32 i, j;
    s32 *ai = (s32 *)&m->m[0][0];
    s32 *af = (s32 *)&m->m[2][0];

    for (i = 0; i < 3; i++) {
        for (j = 0; j < 3; j++) e[i][j] = (s32)(ax[i][j] * 65536.0f);
        e[i][3] = 0;
    }
    for (j = 0; j < 3; j++) e[3][j] = (s32)(t[j] * 65536.0f);
    e[3][3] = 0x10000;

    for (i = 0; i < 4; i++) {
        for (j = 0; j < 2; j++) {
            s32 e1 = e[i][j * 2], e2 = e[i][j * 2 + 1];
            *(ai++) = (e1 & 0xFFFF0000) | ((e2 >> 16) & 0xFFFF);
            *(af++) = ((e1 << 16) & 0xFFFF0000) | (e2 & 0xFFFF);
        }
    }
}

void nb_itoa(char *dst, s32 v) {
    char tmp[12];
    s32 n = 0, neg = v < 0;
    u32 u = neg ? (u32)(-v) : (u32)v;
    do { tmp[n++] = '0' + (u % 10); u /= 10; } while (u && n < 11);
    if (neg) *dst++ = '-';
    while (n) *dst++ = tmp[--n];
    *dst = 0;
}

char *nb_strcpy(char *dst, const char *src) {
    char *d = dst;
    while ((*d++ = *src++)) {}
    return dst;
}

char *nb_strcat(char *dst, const char *src) {
    char *d = dst;
    while (*d) d++;
    while ((*d++ = *src++)) {}
    return dst;
}

void nb_memset(void *dst, s32 v, u32 n) {
    u8 *d = dst;
    while (n--) *d++ = (u8)v;
}

void nb_memcpy(void *dst, const void *src, u32 n) {
    u8 *d = dst;
    const u8 *s = src;
    while (n--) *d++ = *s++;
}

/* GCC may emit calls to these for struct copies / zeroing. */
void *memcpy(void *dst, const void *src, u32 n) { nb_memcpy(dst, src, n); return dst; }
void *memset(void *dst, s32 v, u32 n) { nb_memset(dst, v, n); return dst; }
