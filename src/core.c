#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

struct Font *headerFont;
struct Font *textFont;

static const f32 topPadding = 42.0;
static const f32 bottomPadding = 24.0;
static const f32 trayPadding = 8.0;

#define RGB(r, g, b) (u32)(r | (u32)(g << 8) | (u32)(b << 16) | (u32)(0xFF << 24))
#define RGBA(r, g, b, a) (u32)(r | (g << 8) | (b << 16) | (a << 24))

enum Key {
    K_Up,
    K_Down,
    K_Left,
    K_Right,

    K_Escape,

    _K_COUNT
};

enum MouseButtons {
    MB_Left,
    MB_Right,
    MB_Middle,
    MB_4,
    MB_5,

    _MB_COUNT
};

struct Input {
    f32 mouseX, mouseY;
    f32 scrollX, scrollY;
    f32 zoomX, zoomY;
    f32 dragStartX, dragStartY;
    u8 mouseButtons[_MB_COUNT];
    u8 keys[_K_COUNT];
    b8 shift, alt, control, isDragging, wasDragging;
};

struct Time {
    f64 dt, global;
};

struct Frame {
    u64 offset;
    u64 viewport;
    u64 timeline;

    u64 targetOffset;
    u64 targetViewport;
};

enum Mode {
    Mode_Login,
    Mode_Boards,
    Mode_Board
};

struct State {
    struct Frame frame;
    enum Mode mode;
};

struct Memory {
    void *buffer;
    void *scratch;
};

enum Label {
    Label_Green,
    Label_Yellow,
    Label_Orange,
    Label_Red,
    Label_Purple,
    Label_Blue,
    Label_Sky,
    Label_Lime,
    Label_Pink,
    Label_Black,

    _LABEL_COUNT
};

INLINE b32 KeyPressed(struct Input *input, enum Key key) {
    return input->keys[key];
}

void DefaultState(struct State *state) {
    state->mode = Mode_Boards;
}

void DrawTray(
    struct RenderCommands *commands,
    const char *name,
    v2 pos,
    f32 height
) {
    static const f32 inset = 8.0;
    static const f32 scrollWidth = 8.0;
    static const f32 nameHeight = 32.0;

    static const f32 trayWidth = 280.0;
    static const f32 cardWidth = trayWidth - (inset * 2.0) - scrollWidth;

    const f32 cardsStartX = pos.x + inset;
    const f32 cardsStartY = pos.y + nameHeight + inset;

    static const u32 trayColor = RGB(0xe2, 0xe4, 0xe6);
    static const u32 shadowColor = RGB(0xde, 0xde, 0xde);

    PushRect(commands, V2(pos.x-1, pos.y-1), V2(282, height+2), QuadKind_Normal, shadowColor);
    PushRect(commands, pos, V2(280, height), QuadKind_Normal, trayColor);

    static const u32 textColor = RGB(0x3, 0x3, 0x3);
    DrawText(commands, V2(pos.x + 4, pos.y+20), textColor, headerFont, name);

    static const u32 cardColor = RGB(0xff, 0xff, 0xff);

    static const char *cardNames[] = {
        "Template system",
        "Project dashboard",
        "(3) Send a pulse \"once\"",
        "Import projects",
        "People scale.png"
    };

    f32 yOffset = cardsStartY;
    for (size_t i = 0; i < 5; i += 1) {
        PushRect(commands, V2(cardsStartX-1, yOffset-1), V2(cardWidth+2, 102), QuadKind_Normal, shadowColor);
        PushRect(commands, V2(cardsStartX, yOffset), V2(cardWidth, 100), QuadKind_Normal, cardColor);
        DrawText(commands, V2(cardsStartX + 4, yOffset+12), textColor, textFont, cardNames[i]);

        yOffset += 100 + inset;
    }
}

void DrawSpinner(
    struct RenderCommands *commands,
    f32 originX,
    f32 originY,
    f32 size,
    f32 time
) {
    for (size_t i = 0; i < 8; i += 1) {
        const f32 angle = time * (2.0*3.14159) + (0.7853975 * i);
        const f32 x = (size * cos(angle)) - (size * sin(angle));
        const f32 y = (size * cos(angle)) + (size * sin(angle));
        const u32 color = RGBA(0xb3, 0xb3, 0xb3, i * 255/8);
        PushRect(commands, V2(x+originX, y+originY), V2(10, 10), QuadKind_Circle, color);
    }
}

void tickBoards(
    struct State *state,
    struct Time *time,
    struct Input *input,
    struct Memory *memory,
    struct RenderCommands *commands
) {
    if (KeyPressed(input, K_Right)) {
        state->mode = Mode_Board;
    }

    f32 halfWidth = commands->settings.width/2.0;
    f32 halfHeight = commands->settings.height/2.0;
    DrawSpinner(commands, halfWidth, halfHeight, 16.0, (f32)time->global);
}

void tickBoard(
    struct State *state,
    struct Time *time,
    struct Input *input,
    struct Memory *memory,
    struct RenderCommands *commands
) {

    if (KeyPressed(input, K_Left)) {
        state->mode = Mode_Boards;
    }

    const f32 span = 280 + 8;
    for (size_t i = 0; i < 10; i += 1) {
        DrawTray(commands, "Resources", V2(trayPadding + (i*span), topPadding), commands->settings.height - topPadding - bottomPadding);
    }
}

void Tick(
    struct State *state,
    struct Time *time,
    struct Input *input,
    struct Memory *memory,
    struct RenderCommands *commands,
    b32 *running
) {
    if (KeyPressed(input, K_Escape)) {
        *running = false;
        return;
    }

    headerFont = commands->settings.headerFont;
    textFont = commands->settings.textFont;

    static const u32 textColor = 0xffe8ded9;
    static const u32 line = 0xff6b574d;
    static const u32 bar0 = 0xff5e4d42; // Dark blue
    static const u32 bar1 = 0xffab825e; // Blue
    static const u32 bar2 = 0xff8cbfa3; // Green
    static const u32 bar3 = 0xffad8fb5; // Purple
    static const u32 bar4 = 0xff80cceb; // Yellow
    static const u32 bar5 = 0xff7087d1; // Orange
    static const u32 bar6 = 0xff6b61bf; // Red

    static const u32 topBar = RGB(0x02, 0x6a, 0xa7);

    v4 clearColor = state->mode == Mode_Boards ? V4(0.97, 0.98, 0.98, 1) : V4(0.0, 121.0/255.0, 191.0/255.0, 1.0);
    PushClear(commands, clearColor);

    PushRect(commands, V2(0, 0), V2(commands->settings.width, topPadding-8), QuadKind_Normal, topBar);
    
    switch (state->mode) {
    case Mode_Boards: {
        tickBoards(state, time, input, memory, commands);
    } break;

    case Mode_Board: {
        tickBoard(state, time, input, memory, commands);
    } break;
    }
 
}
