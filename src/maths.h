#include <math.h>

#define TAU 6.28318530718f
#define SWAP(x, y) do { tmp = x; x = y; y = tmp; } while (0)

INLINE f32 min(f32 a, f32 b) {
    return a < b ? a : b;
}

INLINE f32 max(f32 a, f32 b) {
    return a > b ? a : b;
}

INLINE f32 lerp(f32 x, f32 y, f32 t) {
    return (y - x) * t + x;
}

INLINE f32 clamp(f32 x, f32 min, f32 max) {
    if (x < min) return min;
    if (x > max) return max;
    return x;
}

INLINE f32 Abs(f32 a) {
    return fabsf(a);
}

INLINE v2 V2(f32 x, f32 y) {
    v2 res;
    res.x = x;
    res.y = y;
    return res;
}

INLINE v3 V3(f32 x, f32 y, f32 z) {
    v3 res;
    res.x = x;
    res.y = y;
    res.z = z;
    return res;
}

INLINE v4 V4(f32 x, f32 y, f32 z, f32 w) {
    v4 res;
    res.x = x;
    res.y = y;
    res.z = z;
    res.w = w;
    return res;
}

INLINE b32 vCmp(v3 a, v3 b) {
    return (a.x == b.x) && (a.y == b.y) && (a.z == b.z);
}

INLINE v3 vMul(v3 a, v3 b) {
    v3 vec = { a.x * b.x, a.y * b.y, a.z * b.z };
    return vec;
}

INLINE f32 vDot(v3 a, v3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

INLINE f32 vLen2(v3 a) {
    return vDot(a, a);
}

INLINE f32 vLen(v3 a) {
    return sqrtf(vLen2(a));
}

INLINE v3 vNorm(v3 a) {
    if (vLen(a) > 0.0f) {
        f32 l = 1.0f/vLen(a);
        v3 b = { l, l, l };
        return vMul(a, b);
    }

    v3 vec = { 0, 0, 0 };
    return vec;
}

INLINE v3 vCross(v3 a, v3 b) {
    v3 vec = { a.y * b.z - b.y * a.z, b.x * a.z - a.x * b.z, a.x * b.y - b.x * a.y };
    return vec;
}
