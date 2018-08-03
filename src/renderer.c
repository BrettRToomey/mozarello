#define VERTS_PER_QUAD (6)

enum RenderEntryType {
    RenderEntryType_Clear,
    RenderEntryType_Quads,
    RenderEntryType_TexturedQuads
};

struct RenderEntryHeader {
    u32 type;
};

struct RenderEntryClear {
   v4 clearColor; 
};

enum QuadKind {
    QuadKind_Normal,
    QuadKind_Dashed,
    QuadKind_Circle
};

struct RenderEntryQuads {
    u32 count;
    u32 vertexIndex;
    enum QuadKind kind;
};

struct RenderEntryTexturedQuads {
    u32 count;
    u32 vertexIndex;
    enum Palette color;
    Texture textureId;
}__attribute((packed));

struct PushBufferResult {
    struct RenderEntryHeader *header;
};

INLINE
struct PushBufferResult PushBuffer(struct RenderCommands *commands, u32 size) {
    struct PushBufferResult result = {0};
    if ((commands->commandIndex + size) <= commands->commandBufferSize) {
        void *index = commands->commandBuffer + commands->commandIndex;
        result.header = (struct RenderEntryHeader *)index;
        commands->commandIndex += size;
    } else {
        PANIC("Render command buffer is full");
    }

    return result;
}

#define PushRenderElement(commands, type) (struct RenderEntry##type *)PushRenderElement_(commands, sizeof(struct RenderEntry##type), RenderEntryType_##type)
INLINE
void * PushRenderElement_(struct RenderCommands *commands, u32 size, enum RenderEntryType type) {
    void *result = 0;

    size += sizeof(struct RenderEntryHeader);
    struct PushBufferResult pushResult = PushBuffer(commands, size);
    if (pushResult.header) {
        struct RenderEntryHeader *header = pushResult.header;
        header->type = (u16) type;
        result = (u8 *)header+sizeof(*header);
    } else {
        PANIC("Couldn't push buffer");
    }

    commands->currentQuads = NULL;
    commands->currentTexturedQuads = NULL;

    return result;
}

INLINE
void PushClear(struct RenderCommands *commands, v4 color) {
    struct RenderEntryClear *entry = PushRenderElement(commands, Clear);
    if (entry) {
        entry->clearColor = color;
    }
}

INLINE
struct RenderEntryQuads *GetQuads(struct RenderCommands *commands, enum QuadKind kind, u32 count) {
    if (!commands->currentQuads || commands->currentQuads->kind != kind) {
        commands->currentQuads = PushRenderElement(commands, Quads);
        commands->currentQuads->count = 0;
        commands->currentQuads->vertexIndex = commands->vertexCount;
        commands->currentQuads->kind = kind;
    }

    struct RenderEntryQuads *quads = commands->currentQuads;
    if (commands->vertexCount + (count * VERTS_PER_QUAD) > commands->vertexBufferSize) {
        quads = NULL;
    }

    return quads;
}

INLINE
struct RenderEntryTexturedQuads *GetTexturedQuads(struct RenderCommands *commands, Texture textureId, enum Palette color, u32 count) {
    if (!commands->currentTexturedQuads || color != commands->currentTexturedQuads->color) {
        commands->currentTexturedQuads = PushRenderElement(commands, TexturedQuads);
        commands->currentTexturedQuads->count = 0;
        commands->currentTexturedQuads->vertexIndex = commands->texturedVertCount;
        commands->currentTexturedQuads->textureId = textureId;
        commands->currentTexturedQuads->color = color;
    }

    struct RenderEntryTexturedQuads *quads = commands->currentTexturedQuads;
    if (commands->texturedVertCount + (count * VERTS_PER_QUAD) > commands->texturedVertBufferSize) {
        quads = NULL;
    }

    return quads;
}

INLINE
void PushRect(struct RenderCommands *commands, v2 pos, v2 dim, enum QuadKind kind, enum Palette color) {
    struct RenderEntryQuads *quads = GetQuads(commands, kind, 1);
    if (quads) {
        u32 vertIndex = commands->vertexCount;
        commands->vertexCount += VERTS_PER_QUAD;
        quads->count++;

        v2 p1, p2, p3, p4;
        p1 = pos;
        p2 = V2(pos.x,         pos.y + dim.y);
        p3 = V2(pos.x + dim.x, pos.y);
        p4 = V2(pos.x + dim.x, pos.y + dim.y);

        struct Vert *verts = commands->vertexBuffer + vertIndex;
        verts[0].pos = p1;
        verts[0].color = color;

        verts[1].pos = p2;
        verts[1].color = color;

        verts[2].pos = p3;
        verts[2].color = color;

        verts[3].pos = p3;
        verts[3].color = color;

        verts[4].pos = p4;
        verts[4].color = color;

        verts[5].pos = p2;
        verts[5].color = color;
    }
}

INLINE
void PushTexturedRect(struct RenderCommands *commands, v2 pos, v2 dim, v2 uvStart, v2 uvEnd, Texture textureId, enum Palette color) {
    struct RenderEntryTexturedQuads *quads = GetTexturedQuads(commands, textureId, color, 1);
    if (quads) {
        u32 vertIndex = commands->texturedVertCount;
        commands->texturedVertCount += VERTS_PER_QUAD;
        quads->count++;

        v2 p1, p2, p3, p4;
        p1 = pos;
        p2 = V2(pos.x,         pos.y + dim.y);
        p3 = V2(pos.x + dim.x, pos.y);
        p4 = V2(pos.x + dim.x, pos.y + dim.y);

        v2 u1, u2, u3, u4;
        u1 = uvStart;
        u2 = V2(uvStart.x, uvEnd.y);
        u3 = V2(uvEnd.x, uvStart.y);
        u4 = V2(uvEnd.x, uvEnd.y);

        struct TexturedVert *verts = commands->texturedVertBuffer + vertIndex;
        verts[0].pos = p1;
        verts[1].pos = p2;
        verts[2].pos = p3;
        verts[3].pos = p3;
        verts[4].pos = p4;
        verts[5].pos = p2;

        verts[0].uv = u1;
        verts[1].uv = u2;
        verts[2].uv = u3;
        verts[3].uv = u3;
        verts[4].uv = u4;
        verts[5].uv = u2;
    } 
}

/*
 * TODO: To support ligatures, prepare to cry:
 *
 * Start here: https://www.freetype.org/freetype2/docs/reference/ft2-truetype_tables.html#FT_Load_Sfnt_Table
 * We need to load this table:
 * 'GSUB' 0x47535542
 *
 * And search for these features
 * "smcp" = small caps
 * "liga" = ligatures
 * "lnum" = lining numbers
 *
 *  MS has this: https://docs.microsoft.com/en-us/typography/opentype/spec/gsub
 */
void DrawText(struct RenderCommands *commands, v2 origin, enum Palette color, struct Font *font, const char *msg) {
    size_t len = strlen(msg);
    struct RenderEntryTexturedQuads *quads = GetTexturedQuads(commands, font->textureId, color, (u32)len);
    if (quads) {
        u32 vertIndex = commands->texturedVertCount;
        commands->texturedVertCount += len * VERTS_PER_QUAD;
        quads->count += len;

        struct TexturedVert *verts = commands->texturedVertBuffer + vertIndex;

        for (size_t i = 0; i < len; i++) {
            stbtt_aligned_quad align;
            stbtt_GetPackedQuad(font->chars, 1024, 1024, msg[i]-32, &origin.x, &origin.y, &align, 0);

            f32 x0 = align.x0;
            f32 x1 = align.x1;
            f32 y0 = align.y0;
            f32 y1 = align.y1;

            f32 s0 = align.s0;
            f32 s1 = align.s1;
            f32 t0 = align.t0;
            f32 t1 = align.t1;

            verts[0].pos = V2(x0, y0);
            verts[1].pos = V2(x0, y1);
            verts[2].pos = V2(x1, y0);
            verts[3].pos = V2(x1, y0);
            verts[4].pos = V2(x1, y1);
            verts[5].pos = V2(x0, y1);

            verts[0].uv = V2(s0, t0);
            verts[1].uv = V2(s0, t1);
            verts[2].uv = V2(s1, t0);
            verts[3].uv = V2(s1, t0);
            verts[4].uv = V2(s1, t1);
            verts[5].uv = V2(s0, t1);

            verts += VERTS_PER_QUAD;
        }
    }
}
