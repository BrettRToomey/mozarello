#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "stb_truetype.h"

#if defined(_WINDOWS) || defined(_WIN32)
    #define PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__FreeBSD__)
    #define PLATFORM_LINUX
    #define PLATFORM_POSIX
#elif defined(__APPLE__)
    #define PLATFORM_MACOS
    #define PLATFORM_POSIX
#endif

// https://stackoverflow.com/a/22291538
//#if defined(_MSC_VER)
//     /* Microsoft C/C++-compatible compiler */
//     #include <intrin.h>
//#elif defined(__GNUC__) && (defined(__x86_64__) || defined(__i386__))
//     /* GCC-compatible compiler, targeting x86/x86-64 */
//     #include <x86intrin.h>
//#elif defined(__GNUC__) && defined(__ARM_NEON__)
//     /* GCC-compatible compiler, targeting ARM with NEON */
//     #include <arm_neon.h>
//#elif defined(__GNUC__) && defined(__IWMMXT__)
//     /* GCC-compatible compiler, targeting ARM with WMMX */
//     #include <mmintrin.h>
//#elif (defined(__GNUC__) || defined(__xlC__)) && (defined(__VEC__) || defined(__ALTIVEC__))
//     /* XLC or GCC-compatible compiler, targeting PowerPC with VMX/VSX */
//     #include <altivec.h>
//#elif defined(__GNUC__) && defined(__SPE__)
//     /* GCC-compatible compiler, targeting PowerPC with SPE */
//     #include <spe.h>
//#endif

#ifdef PLATFORM_MACOS
    #include <mach/mach_time.h>
    #include <mach/vm_map.h>
    #include <mach/mach.h>
    #include <sys/time.h>
#else
    #ifndef __FreeBSD__
        #include <malloc.h>
    #endif
#endif

#include <assert.h>

#ifdef PLATFORM_WINDOWS
    #include <winsock2.h>
    #ifndef __MINGW32__
        #include <intrin.h>
    #endif
    #undef min
    #undef max
    #ifdef _XBOX_ONE
        #include "xmem.h"
    #endif
#endif

#ifdef PLATFORM_LINUX
    #include <time.h>
    #ifdef __FreeBSD__
        #include <pthread_np.h>
    #else
        #include <sys/prctl.h>
    #endif
#endif

#if defined(PLATFORM_POSIX)
    #include <stdlib.h>
    #include <pthread.h>
    #include <unistd.h>
    #include <string.h>
    #include <arpa/inet.h>
    #include <sys/socket.h>
    #include <sys/mman.h>
    #include <netinet/in.h>
    #include <fcntl.h>
    #include <errno.h>
    #include <dlfcn.h>
#endif

#ifdef __MINGW32__
    #include <pthread.h>
#endif

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))
#define KB(x) (  (x)*1024LL)
#define MB(x) (KB(x)*1024LL)
#define GB(x) (MB(x)*1024LL)
#define TB(x) (GB(x)*1024LL)

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef int8_t   i8;
typedef int16_t  i16;
typedef int32_t  i32;
typedef int64_t  i64;

typedef float    f32;
typedef double   f64;

typedef i8       b8;
typedef i32      b32;

#define true  1
#define false 0

#if defined(_MSC_VER)
    #if _MSC_VER < 1300
        #define DEBUG_TRAP() __asm int 3
    #else
        #define DEBUG_TRAP() __debugbreak()
    #endif
#else
    #define DEBUG_TRAP() __builtin_trap()
#endif

#ifndef RELEASE
    #define ASSERT_MSG_VA(cond, msg, ...) do { \
        if (!(cond)) { \
            assert_handler(__FILE__, (i64)__LINE__, msg, __VA_ARGS__); \
            DEBUG_TRAP(); \
        } \
    } while(0)

    #define ASSERT_MSG(cond, msg) ASSERT_MSG_VA(cond, msg, 0)

    #define ASSERT(cond) ASSERT_MSG_VA(cond, 0, 0)
    #define PANIC(msg) ASSERT_MSG_VA(0, msg, 0)
    #define UNIMPLEMENTED() ASSERT_MSG_VA(0, "unimplemented", 0);
#else
    #define ASSERT_MSG_VA(cond, msg, ...)
    #define ASSERT_MSG(cond, msg)
    #define ASSERT(cond)
    #define PANIC(msg)
    #define UNIMPLEMENTED()
#endif
void assert_handler(char const *file, i32 line, char const *msg, ...) {
    if (msg)
        printf("Assert failure: %s:%d: %s\n", file, line, msg);
    else
        printf("Assert failure: %s:%d\n", file, line);
}

enum AllocType {
    AT_Alloc,
    AT_Free,
    AT_FreeAll,
    AT_Resize
};

#define ALLOC_FUNC(name) void *name(void *payload, enum AllocType type, u64 size, u64 oldSize, void *old)
typedef ALLOC_FUNC(AllocFunc);

struct Allocator {
    AllocFunc *func;
    void *userdata;
};

ALLOC_FUNC(defaultHeapAllocFunc);
ALLOC_FUNC(arenaAllocFunc);

struct Allocator DefaultHeapAllocator() {
    struct Allocator a = {
        defaultHeapAllocFunc,
        0
    };
    return a;
}

ALLOC_FUNC(defaultHeapAllocFunc) {
    switch (type) {
    case AT_Alloc: {
        return malloc(size);
    } break;

    case AT_Free: {
        free(old);
    } break;

    case AT_FreeAll: {
    } break;

    case AT_Resize: {
        return realloc(old, size);
    } break;
    }

    return NULL;
}

void *Alloc(struct Allocator a, u64 size) {
    return a.func(a.userdata, AT_Alloc, size, 0, 0);
}

void Free(struct Allocator a, void *ptr) {
    if (!ptr) return;
    a.func(a.userdata, AT_Free, 0, 0, ptr);
}

void FreeAll(struct Allocator a) {
    a.func(a.userdata, AT_FreeAll, 0, 0, 0);
}

void *Resize(struct Allocator a, void *ptr, u64 oldSize, u64 size) {
    return a.func(a.userdata, AT_Resize, size, oldSize, ptr);
}

struct ArrayHeader {
    struct Allocator allocator;
    u64 len, cap;
};

#define Array(Type) Type *

#define ARRAY_GROW(x)     (2*(x) + 8)
#define ARRAY_HEADER(x)   ((ArrayHeader *)(x) - 1)
#define ArrayAllocator(x) (ARRAY_HEADER(x)->allocator)
#define ArrayLen(x)       (ARRAY_HEADER(x)->len)
#define ArrayCap(x)       (ARRAY_HEADER(x)->cap)
#define INLINE static inline

#define ArrayClear(x) do { ARRAY_HEADER(x)->len = 0; } while (0)
#define ArrayPop(x)   do { ARRAY_HEADER(x)->len -= 1; } while(0)

#define ArrayInitReserve(x, _allocator, _cap) do { \
    void **_array = (void **)&(x); \
    struct ArrayHeader *_ah = Alloc(_allocator, sizeof(struct ArrayHeader)+sizeof(*(x))*(_cap)); \
    _ah->allocator = _allocator; \
    _ah->len = 0; \
    _ah->cap = _cap; \
    *_array = (void *)(_ah+1); \
} while (0)

#define ArrayInit(x, allocator) ArrayInitReserve(x, allocator, ARRAY_GROW(0))

#define JOIN2(a, b) a##b
#define TABLE(name, value) \
    TABLE_DECLARE(name, value); \
    TABLE_DEFINE(name, value);

#define TABLE_DECLARE(name, value) \
    struct name { \
        u64 *keys; \
        value *values; \
        u64 len, cap; \
        struct Allocator allocator; \
    } \
\
void   JOIN2(name, Init) (name *h, struct Allocator a); \
value *JOIN2(name, Get)  (name *h, u64 key); \
void   JOIN2(name, Set)  (name *h, u64 key, value val); \
void   JOIN2(name, Grow) (name *h);

#define TABLE_DEFINE(name, value) \
void JOIN2(name, Init)(name *h, struct Allocator a) { \
    h->keys = Alloc(a, sizeof(u64) * 100); \
    h->values = Alloc(a, sizeof(value) * 100); \
    h->len = 0; \
    h->cap = 100; \
    h->allocator = a; \
} \
\
value *JOIN2(name, Get)(name *h, u64 key) { \
   \
}

u64 fnv64a(void const *data, u64 len) {
	u64 i;
	u64 h = 0xcbf29ce484222325ull;
	u8 const *c = (u8 const *)data;

	for (i = 0; i < len; i++) {
		h = (h ^ c[i]) * 0x100000001b3ll;
	}

	return h;
}

struct MapFind {
    i64 hash;
    i64 entry;
};

struct U32Map {
    u64 *keys;
    u32 *values;
    u64 len, cap;
    struct Allocator allocator;
};

void U32MapInit(struct U32Map *self, struct Allocator a, u64 cap) {
    self->keys = Alloc(a, sizeof(u64)*cap);
    self->values = Alloc(a, sizeof(u32)*cap);
    self->len = 0;
    self->cap = cap;
    self->allocator = a;
}

struct MapFind U32MapFind(struct U32Map *self, u64 key) {
    struct MapFind f = {-1, -1};

    u64 index = key % self->cap;
    u64 k = self->keys[index];
    f.hash = (i64)index;

    if (self->len > 0) {
        while (k != 0) {
            if (k == key) {
                f.entry =(i64)index;
                return f;
            }

            index = (index+1)%self->cap;
            k = self->keys[index];
        }
    }

    return f;
}

u32 *U32MapGet(struct U32Map *self, u64 key) {
    i64 index = U32MapFind(self, key).entry;
    if (index >= 0)
        return &self->values[index];
    return NULL;
}

void U32MapSet(struct U32Map *self, u64 key, u32 value) {
    i64 index;
    struct MapFind f;
    f = U32MapFind(self, key);
    if (f.entry >= 0) {
        index = f.entry;
    } else {
        index = f.hash;
        self->keys[index] = key;
        self->len += 1;
    }

    self->values[index] = value;
}

void U32MapRemove(struct U32Map *self, u64 key) {
    i64 index = U32MapFind(self, key).entry;
    if (index >= 0) {
        self->keys[index] = 0;
        self->len -= 1;
    }
}

#define InvalidCodePoint 0xFFFD

// WARNING: this function cannot handle a buffer where `len = 0`
u32 DecodeCodePoint(u32 *cpLen, u8 *buffer) {
    static const u32 FIRST_LEN[] = {
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
        4, 4, 4, 4, 4, 4, 4, 4, 1, 1, 1, 1, 1, 1, 1, 1
    };

    static const u8 MASK[] = {
        0xFF, 0xFF, 0x1F, 0xF, 0x7
    };

    u8 b0 = buffer[0];
    int l = FIRST_LEN[b0];
    int val = (int)(b0 & MASK[l]);

    for (int i=1; i < l; i += 1) {
        val = (val << 6) | (int)(buffer[i] & 0x3f);
    }

    if (cpLen)
        *cpLen = l;
    return val;
}

// WARNING: this function assumes that buffer is >= 4 bytes
u32 EncodeCodePoint(u8 * const buffer, const u32 cp) {
    if (cp <= 0x7F) {
        buffer[0] = cp;
        return 1;
    }

    if (cp <= 0x7FF) {
        buffer[0] = 0xC0 | (cp >> 6);
        buffer[1] = 0x80 | (cp & 0x3F);
        return 2;
    }

    if (cp <= 0xFFFF) {
        buffer[0] = 0xE0 | (cp >> 12);
        buffer[1] = 0x80 | ((cp >> 6) & 0x3F);
        buffer[2] = 0x80 | (cp & 0x3F);
        return 3;
    }

    if (cp <= 0x10FFFF) {
        buffer[0] = 0xF0 | (cp >> 18);
        buffer[1] = 0x80 | ((cp >> 12) & 0x3F);
        buffer[2] = 0x80 | ((cp >> 6) & 0x3F);
        buffer[3] = 0x80 | (cp & 0x3F);
        return 4;
    }

    return 0;
}

u32 InsertCodePoint(size_t index, const u32 cp, u8 *buffer, size_t blen, size_t endIndex) {
    u32 len;
    u8 buf[4];
    len = EncodeCodePoint(&buf[0], cp);

    size_t newEnd = endIndex+len;
    if (newEnd >= blen)
        return 0;

    // move old bytes to their new spot
    for (size_t i = endIndex; i >= index; i -= 1) {
        buffer[i+len] = buffer[i];
    }

    memcpy(&buffer[index], &buf[0], len);

    return len;
}

u32 RemoveCodePoint(size_t index, u8 *buffer, size_t endIndex) {
    u32 len;
    DecodeCodePoint(&len, &buffer[index]);
    for (size_t i = index; i <= (endIndex-len); i += 1) {
        buffer[i] = buffer[i+len];
    }
    return len;
}

struct Glyph {
    u32 advance;
    u32 uv[2];
    u32 size[2];
    u32 bearing[2];
};

typedef void * Texture;
struct Font {
    Texture textureId;
    const stbtt_packedchar *chars;
};

typedef union {
    struct {
        f32 x, y;
    };

    struct {
        f32 u, v;
    };
    f32 data[2];
} v2;

typedef union {
    struct {
        f32 x, y, z;
    };

    struct {
        f32 u, v, __;
    };

    struct {
        f32 r, g, b;
    };

    struct {
        v2 xy;
        f32 ignored0__;
    };

    struct {
        f32 ignored1__;
        v2 yz;
    };

    struct {
        v2 uv;
        f32 ignored2__;
    };

    f32 data[3];
} v3;

typedef union {
    struct {
        union {
            v3 xyz;
            struct {
                f32 x, y, z;
            };
        };

        f32 w;
    };

    struct {
        union {
            v3 rgb;
            struct {
                f32 r, g, b;
            };
        };

        f32 a;
    };

    struct {
        v2 xy;
        f32 ignored0__;
        f32 ignored1__;
    };

    struct {
        f32 ignored2__;
        v2 yz;
        f32 ignored3__;
    };

    struct {
        f32 ignored4__;
        f32 ignored5__;
        v2 zw;
    };

    f32 data[4];
} v4;

struct mat4 {
    f32 data[4][4];
};

b32 CmpV4(v4 lhs, v4 rhs) {
    return lhs.x == rhs.x && lhs.y == rhs.y && lhs.z == rhs.z && lhs.w == rhs.w;
}

static unsigned GetTimems() {
#ifdef PLATFORM_WINDOWS
    return (unsigned)GetTickCount();
#else
    clock_t time = clock();

    // CLOCKS_PER_SEC is 128 on FreeBSD, causing div/0
#ifdef __FreeBSD__
    unsigned msTime = (unsigned) (time * 1000 / CLOCKS_PER_SEC);
#else
    unsigned msTime = (unsigned) (time / (CLOCKS_PER_SEC / 1000));
#endif

    return msTime;
#endif
}

typedef struct {
    unsigned long counter_start;
    double counter_scale;
} usTimer;

static void usTimerInit(usTimer* timer) {
#if defined(PLATFORM_WINDOWS)
    unsigned long performance_frequency;

    assert(timer != NULL);

    // Calculate the scale from performance counter to microseconds
    QueryPerformanceFrequency(&performance_frequency);
    timer->counter_scale = 1000000.0 / performance_frequency.QuadPart;

    // Record the offset for each read of the counter
    QueryPerformanceCounter(&timer->counter_start);

#elif defined(PLATFORM_MACOS)
    mach_timebase_info_data_t nsScale;
    mach_timebase_info( &nsScale );
    const double ns_per_us = 1.0e3;
    timer->counter_scale = (double)(nsScale.numer) / ((double)nsScale.denom * ns_per_us);
    timer->counter_start = mach_absolute_time();

#elif defined(PLATFORM_LINUX)
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    timer->counter_start = (unsigned long)(tv.tv_sec * (unsigned long)1000000) + (unsigned long)(tv.tv_nsec * 0.001);

#endif
}

static unsigned long GetTimeus(usTimer* timer) {
#if defined(PLATFORM_WINDOWS)
    unsigned long performance_count;

    assert(timer != NULL);

    // Read counter and convert to microseconds
    QueryPerformanceCounter(&performance_count);
    return (unsigned long)((performance_count.QuadPart - timer->counter_start.QuadPart) * timer->counter_scale);

#elif defined(PLATFORM_MACOS)
    unsigned long curr_time = mach_absolute_time();
    return (unsigned long)((curr_time - timer->counter_start) * timer->counter_scale);

#elif defined(PLATFORM_LINUX)
    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return  ((unsigned long)(tv.tv_sec * (unsigned long)1000000) + (unsigned long)(tv.tv_nsec * 0.001)) - timer->counter_start;

#endif
}

enum Palette {
    Palette_Transparent,
    Palette_Text,
    Palette_TextInverted,
    Palette_Line,

    _PALETTE_COUNT
};

struct Vert {
    v2 pos;
    enum Palette color;
};

struct TexturedVert {
    v2 pos;
    v2 uv;
};

struct RenderSettings {
    u32 width;
    u32 height;
    struct Font *headerFont;
    struct Font *textFont;
};

struct RenderEntryQuads;
struct RenderEntryTexturedQuads;

struct RenderCommands {
    struct RenderSettings settings;

    u32 commandBufferSize;
    u32 commandIndex;
    u8 *commandBuffer;

    u32 vertexBufferSize;
    u32 vertexCount;
    struct Vert *vertexBuffer;

    u32 texturedVertBufferSize;
    u32 texturedVertCount;
    struct TexturedVert *texturedVertBuffer;


    struct RenderEntryQuads *currentQuads;
    struct RenderEntryTexturedQuads *currentTexturedQuads;
};

INLINE struct RenderCommands RenderCommandsInit(
    u32 commandBufferSize,
    u8 *commandBuffer,
    u32 vertexBufferSize,
    struct Vert *vertexBuffer,
    u32 texturedVertBufferSize,
    struct TexturedVert *texturedVertBuffer,
    u32 width,
    u32 height
) {
    struct RenderCommands commands = {0};

    commands.settings.width = width;
    commands.settings.height = height;

    commands.commandBufferSize = commandBufferSize;
    commands.commandBuffer = commandBuffer;
    commands.commandIndex = 0;

    commands.currentQuads = NULL;

    commands.vertexBufferSize = vertexBufferSize;
    commands.vertexBuffer = vertexBuffer;
    commands.vertexCount = 0;

    commands.texturedVertBufferSize = texturedVertBufferSize;
    commands.texturedVertBuffer = texturedVertBuffer;
    commands.texturedVertCount = 0;

    return commands;
}

#ifdef __cplusplus
}
#endif
