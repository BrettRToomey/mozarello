#include "glad/glad.h"
#include "GLFW/glfw3.h"

#include "common.h"
#include "maths.h"
#include "renderer.c"
#include "core.c"
#include "glad.c"
#include "opengl.c"

// Target specific imports
#include <time.h>
#include <sys/stat.h>
#include <dlfcn.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <errno.h>

GLFWwindow *window;

static void keyboard_callback(GLFWwindow *w, int key, int scancode, int action, int mods) {
    struct Input *input = glfwGetWindowUserPointer(w);
    u8 state = action != GLFW_RELEASE;

    if (key == GLFW_KEY_ESCAPE)
        input->keys[K_Escape] = state;
    else if (key == GLFW_KEY_UP)
        input->keys[K_Up] = state;
    else if (key == GLFW_KEY_LEFT)
        input->keys[K_Down] = state;
    else if (key == GLFW_KEY_DOWN)
        input->keys[K_Left] = state;
    else if (key == GLFW_KEY_RIGHT)
        input->keys[K_Right] = state;
}

static void mouse_button_callback(GLFWwindow *w, int button, int action, int mods) {
    struct Input *input = glfwGetWindowUserPointer(w);
    u8 state = action != GLFW_RELEASE;
    if (button == GLFW_MOUSE_BUTTON_LEFT)
        input->mouseButtons[MB_Left] = state;
    else if (button == GLFW_MOUSE_BUTTON_RIGHT)
        input->mouseButtons[MB_Right] = state;
    else if (button == GLFW_MOUSE_BUTTON_MIDDLE)
        input->mouseButtons[MB_Middle] = state;
    else if (button == GLFW_MOUSE_BUTTON_4)
        input->mouseButtons[MB_4] = state;
    else if (button == GLFW_MOUSE_BUTTON_5)
        input->mouseButtons[MB_5] = state;
}

static void glfw_err_handler(int code, const char *message) {
    printf("GLFW error: %s\n", message);
}

void *mapMemory(void *memStart, u64 size) {
    void *mem = mmap(memStart, size, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mem == (void *)-1) {
        printf("Failed to grab memory chunk: %s (%d)\n", strerror(errno), errno);
        exit(1);
    }

    return mem;
}

int main() {
    if (!glfwInit())
        return 1;

    glfwSetErrorCallback(glfw_err_handler);

    i32 width = 1280;
    i32 height = 720;
    
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    window = glfwCreateWindow(width, height, "Bench", NULL, NULL);
    if (!window)
        return 2;

    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader((GLADloadproc) glfwGetProcAddress)) {
        printf("GLAD failure\n");
        return 3;
    }

    struct Memory memory = {0};
    struct Input input = {0};
    struct Time time = {0};

    void *memStart = (void *)TB(1);
    void *mem = mapMemory(memStart, MB(100));
    memory.buffer = mem;

    u32 maxVerts = 65536;
    struct Vert *vertBuffer = mapMemory(NULL, maxVerts * sizeof(struct Vert));
    struct TexturedVert *texturedVertBuffer = mapMemory(NULL, maxVerts * sizeof(struct TexturedVert));

    struct RenderCommands renderCommands = RenderCommandsInit(
        KB(1),
        memory.buffer,
        maxVerts,
        vertBuffer,
        maxVerts,
        texturedVertBuffer,
        width,
        height
    );

    glfwSetWindowUserPointer(window, &input);
    glfwSetKeyCallback(window, keyboard_callback);
    glfwSetMouseButtonCallback(window, mouse_button_callback);

    f64 x, y;
    glfwGetCursorPos(window, &x, &y);

    f64 lastTick = glfwGetTime();
    f64 deltaTime, frameTime = 0;
    u32 frameCount, fps = 0;

    b32 running = true;
    while (running) {
        f64 now = glfwGetTime();
        deltaTime = now - lastTick;

        Tick(&time, &input, &memory, &renderCommands, &running);

        OpenGLRenderCommands(&renderCommands);

        glfwSwapBuffers(window);
        glfwPollEvents();

        if (glfwWindowShouldClose(window))
            running = false;

        glfwGetCursorPos(window, &x, &y);
        input.mouseX = (f32)x;
        input.mouseY = (f32)y;

        frameCount += 1;
        if (now - frameTime > 1.0) {
            fps = frameCount;
            frameCount = 0;
            frameTime = now;
        }

        lastTick = now;
    }
    
    return 0;
}
