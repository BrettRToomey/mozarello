#include "glad/glad.h"

static char *SHARED_SHADER = 
    "// Shared code\n"
    "#version 150 core\n";

static char *VERT_SHADER = 
    "// Vertex\n"
    "in vec2 position;"
    "void main() {"
    "   gl_Position = vec4(position, 0.0, 1.0);"
    "}";

static char *FRAG_SHADER = 
    "// Fragment\n"
    "uniform vec3 InColor;"
    "out vec4 outColor;"
    "void main() {"
    "   outColor = vec4(InColor.rgb, 1.0);"
    "}";

GLuint LoadShader(char *sharedCode, char *vertCode, char *fragCode) {
    GLuint vertId, fragId, programId;

    vertId = glCreateShader(GL_VERTEX_SHADER);
    const GLchar * const vertShaderCode[] = {
        sharedCode,
        vertCode
    };
    glShaderSource(vertId, ArrayCount(vertShaderCode), vertShaderCode, 0);
    glCompileShader(vertId);

    fragId = glCreateShader(GL_FRAGMENT_SHADER);
    const GLchar * const fragShaderCode[] = {
        sharedCode,
        fragCode
    };
    glShaderSource(fragId, ArrayCount(fragShaderCode), fragShaderCode, 0);
    glCompileShader(fragId);

    programId = glCreateProgram();
    glAttachShader(programId, vertId);
    glAttachShader(programId, fragId);
    glLinkProgram(programId);

    GLint validated = 0;
    glValidateProgram(programId);
    glGetProgramiv(programId, GL_VALIDATE_STATUS, &validated);
    if (!validated) {
        GLsizei length;
        char vertErrors[4096];
        char fragErrors[4096];
        char progErrors[4096];
        glGetShaderInfoLog(vertId, sizeof(vertErrors), &length, vertErrors);
        glGetShaderInfoLog(fragId, sizeof(fragErrors), &length, fragErrors);
        glGetProgramInfoLog(programId, sizeof(progErrors), &length, progErrors);
        printf("%s\n", vertErrors);
        printf("%s\n", fragErrors);
        printf("%s\n", progErrors);
    }

    glDeleteProgram(vertId);
    glDeleteProgram(fragId);

    return programId;
}

void OpenGLRenderCommands(struct RenderCommands *commands) {
    for (
        u8 *headerIndex = commands->commandBuffer;
        headerIndex < commands->commandBuffer + commands->commandIndex;
        headerIndex += sizeof(struct RenderEntryHeader)
    ) {
        struct RenderEntryHeader *header = (struct RenderEntryHeader *)headerIndex;
        void *payload = (u8 *)header + sizeof(*header);
        switch (header->type) {
        case RenderEntryType_Clear: {
            headerIndex += sizeof(struct RenderEntryClear);
            struct RenderEntryClear *entry = (struct RenderEntryClear *)payload;
            v4 color = entry->clearColor;
            glClearColor(color.r, color.g, color.b, color.a);
            glClear(GL_COLOR_BUFFER_BIT);
        } break;

        case RenderEntryType_Quads: {
            headerIndex += sizeof(struct RenderEntryQuads);
            struct RenderEntryQuads *entry = (struct RenderEntryQuads *)payload;

        } break;

        default: {
            PANIC("Unhandled render command");
        } break;
        }
    }
}

