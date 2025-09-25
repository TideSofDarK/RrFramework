/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Platform.h"

#include "Rr_App.h"
#include "Rr_Vulkan.h"

#include <GLFW/glfw3.h>

static inline GLFWmonitor *Rr_GetGLFWMonitor(void)
{
    GLFWmonitor *Monitor;

    if (!gPlatform->Window)
    {
        Monitor = glfwGetPrimaryMonitor();
    }
    else
    {
        Monitor = glfwGetWindowMonitor(gPlatform->Window);
    }
    if (!Monitor)
    {

        Monitor = glfwGetPrimaryMonitor();
    }

    return Monitor;
}

static inline Rr_Vec2 Rr_GetGLFWCursorPos(void)
{
    double MouseX, MouseY;
    glfwGetCursorPos(gPlatform->Window, &MouseX, &MouseY);
    if (gPlatform->WindowScaled)
    {
        return (Rr_Vec2){ (float)MouseX * gPlatform->WindowScale.X,
                          (float)MouseY * gPlatform->WindowScale.Y };
    }
    return (Rr_Vec2){ (float)MouseX, (float)MouseY };
}

static void Rr_GLFWCursorCallback(GLFWwindow *Window, double X, double Y)
{
    Rr_Platform *RrWindow = glfwGetWindowUserPointer(Window);
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_MOUSE_MOTION;
    if (gPlatform->WindowScaled)
    {
        Event->MouseMotion.Position =
            (Rr_Vec2){ (float)X * gPlatform->WindowScale.X,
                       (float)Y * gPlatform->WindowScale.Y };
    }
    else
    {
        Event->MouseMotion.Position = (Rr_Vec2){ (float)X, (float)Y };
    }
}

static const Rr_Scancode GLFWScancodes[GLFW_KEY_LAST + 1] = {
    [GLFW_KEY_A] = RR_SCANCODE_A,
    [GLFW_KEY_B] = RR_SCANCODE_B,
    [GLFW_KEY_C] = RR_SCANCODE_C,
    [GLFW_KEY_D] = RR_SCANCODE_D,
    [GLFW_KEY_E] = RR_SCANCODE_E,
    [GLFW_KEY_F] = RR_SCANCODE_F,
    [GLFW_KEY_G] = RR_SCANCODE_G,
    [GLFW_KEY_H] = RR_SCANCODE_H,
    [GLFW_KEY_I] = RR_SCANCODE_I,
    [GLFW_KEY_J] = RR_SCANCODE_J,
    [GLFW_KEY_K] = RR_SCANCODE_K,
    [GLFW_KEY_L] = RR_SCANCODE_L,
    [GLFW_KEY_M] = RR_SCANCODE_M,
    [GLFW_KEY_N] = RR_SCANCODE_N,
    [GLFW_KEY_O] = RR_SCANCODE_O,
    [GLFW_KEY_P] = RR_SCANCODE_P,
    [GLFW_KEY_Q] = RR_SCANCODE_Q,
    [GLFW_KEY_R] = RR_SCANCODE_R,
    [GLFW_KEY_S] = RR_SCANCODE_S,
    [GLFW_KEY_T] = RR_SCANCODE_T,
    [GLFW_KEY_U] = RR_SCANCODE_U,
    [GLFW_KEY_V] = RR_SCANCODE_V,
    [GLFW_KEY_W] = RR_SCANCODE_W,
    [GLFW_KEY_X] = RR_SCANCODE_X,
    [GLFW_KEY_Y] = RR_SCANCODE_Y,
    [GLFW_KEY_Z] = RR_SCANCODE_Z,
    [GLFW_KEY_1] = RR_SCANCODE_1,
    [GLFW_KEY_2] = RR_SCANCODE_2,
    [GLFW_KEY_3] = RR_SCANCODE_3,
    [GLFW_KEY_4] = RR_SCANCODE_4,
    [GLFW_KEY_5] = RR_SCANCODE_5,
    [GLFW_KEY_6] = RR_SCANCODE_6,
    [GLFW_KEY_7] = RR_SCANCODE_7,
    [GLFW_KEY_8] = RR_SCANCODE_8,
    [GLFW_KEY_9] = RR_SCANCODE_9,
    [GLFW_KEY_0] = RR_SCANCODE_0,
    [GLFW_KEY_ENTER] = RR_SCANCODE_RETURN,
    [GLFW_KEY_ESCAPE] = RR_SCANCODE_ESCAPE,
    [GLFW_KEY_BACKSPACE] = RR_SCANCODE_BACKSPACE,
    [GLFW_KEY_TAB] = RR_SCANCODE_TAB,
    [GLFW_KEY_SPACE] = RR_SCANCODE_SPACE,
    [GLFW_KEY_CAPS_LOCK] = RR_SCANCODE_CAPSLOCK,
    [GLFW_KEY_F1] = RR_SCANCODE_F1,
    [GLFW_KEY_F2] = RR_SCANCODE_F2,
    [GLFW_KEY_F3] = RR_SCANCODE_F3,
    [GLFW_KEY_F4] = RR_SCANCODE_F4,
    [GLFW_KEY_F5] = RR_SCANCODE_F5,
    [GLFW_KEY_F6] = RR_SCANCODE_F6,
    [GLFW_KEY_F7] = RR_SCANCODE_F7,
    [GLFW_KEY_F8] = RR_SCANCODE_F8,
    [GLFW_KEY_F9] = RR_SCANCODE_F9,
    [GLFW_KEY_F10] = RR_SCANCODE_F10,
    [GLFW_KEY_F11] = RR_SCANCODE_F11,
    [GLFW_KEY_F12] = RR_SCANCODE_F12,
    [GLFW_KEY_HOME] = RR_SCANCODE_HOME,
    [GLFW_KEY_PAGE_UP] = RR_SCANCODE_PAGEUP,
    [GLFW_KEY_DELETE] = RR_SCANCODE_DELETE,
    [GLFW_KEY_END] = RR_SCANCODE_END,
    [GLFW_KEY_PAGE_DOWN] = RR_SCANCODE_PAGEDOWN,
    [GLFW_KEY_RIGHT] = RR_SCANCODE_RIGHT,
    [GLFW_KEY_LEFT] = RR_SCANCODE_LEFT,
    [GLFW_KEY_DOWN] = RR_SCANCODE_DOWN,
    [GLFW_KEY_UP] = RR_SCANCODE_UP,
    [GLFW_KEY_NUM_LOCK] = RR_SCANCODE_NUMLOCKCLEAR,
    [GLFW_KEY_KP_DIVIDE] = RR_SCANCODE_KP_DIVIDE,
    [GLFW_KEY_KP_MULTIPLY] = RR_SCANCODE_KP_MULTIPLY,
    [GLFW_KEY_KP_SUBTRACT] = RR_SCANCODE_KP_MINUS,
    [GLFW_KEY_KP_ADD] = RR_SCANCODE_KP_PLUS,
    [GLFW_KEY_KP_ENTER] = RR_SCANCODE_KP_ENTER,
    [GLFW_KEY_KP_1] = RR_SCANCODE_KP_1,
    [GLFW_KEY_KP_2] = RR_SCANCODE_KP_2,
    [GLFW_KEY_KP_3] = RR_SCANCODE_KP_3,
    [GLFW_KEY_KP_4] = RR_SCANCODE_KP_4,
    [GLFW_KEY_KP_5] = RR_SCANCODE_KP_5,
    [GLFW_KEY_KP_6] = RR_SCANCODE_KP_6,
    [GLFW_KEY_KP_7] = RR_SCANCODE_KP_7,
    [GLFW_KEY_KP_8] = RR_SCANCODE_KP_8,
    [GLFW_KEY_KP_9] = RR_SCANCODE_KP_9,
    [GLFW_KEY_KP_0] = RR_SCANCODE_KP_0,
    [GLFW_KEY_KP_DECIMAL] = RR_SCANCODE_KP_PERIOD,
};

static void Rr_GLFWKeyCallback(
    GLFWwindow *Window,
    int Key,
    int Scancode,
    int Action,
    int Mods)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type =
        Action == GLFW_PRESS ? RR_EVENT_TYPE_KEY_DOWN : RR_EVENT_TYPE_KEY_UP;
    Event->Key.Down = Action == GLFW_PRESS || Action == GLFW_REPEAT;
    Event->Key.Scancode = GLFWScancodes[Key];
    Event->Key.Keymod = 0;
    if (Mods & GLFW_MOD_CONTROL)
    {
        Event->Key.Keymod |= RR_KEYMOD_CTRL;
    }
    if (Mods & GLFW_MOD_SHIFT)
    {
        Event->Key.Keymod |= RR_KEYMOD_SHIFT;
    }
    if (Mods & GLFW_MOD_ALT)
    {
        Event->Key.Keymod |= RR_KEYMOD_ALT;
    }
}

static void Rr_GLFWScrollCallback(GLFWwindow *Window, double X, double Y)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_MOUSE_WHEEL;
    Event->Wheel.Position = Rr_GetGLFWCursorPos();
    Event->Wheel.Amount = (Rr_Vec2){ (float)X, (float)Y };
}

static void Rr_GLFWDropCallback(
    GLFWwindow *Window,
    int32_t Count,
    const char *Paths[])
{
    Rr_Platform *RrWindow = glfwGetWindowUserPointer(Window);
    for (int32_t Index = 0; Index < Count; ++Index)
    {
        size_t Length = strlen(Paths[Index]);
        char *Path = RR_ALLOC_NO_ZERO(RrWindow->EventScratch.Arena, Length + 1);
        memcpy(Path, Paths[Index], Length + 1);

        Rr_Event *Event = Rr_AddEvent();
        Event->Type = RR_EVENT_TYPE_DROP_FILE;
        Event->DropFile.Path = Path;
    }
}

static void Rr_GLFWWindowSizeCallback(GLFWwindow *Window, int Width, int Height)
{
}

static void Rr_GLFWMouseButtonCallback(
    GLFWwindow *Window,
    int Button,
    int Action,
    int Mods)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = Action == GLFW_RELEASE ? RR_EVENT_TYPE_MOUSE_BUTTON_UP
                                         : RR_EVENT_TYPE_MOUSE_BUTTON_DOWN;
    Event->MouseButton.Position = Rr_GetGLFWCursorPos();
    switch (Button)
    {
        case GLFW_MOUSE_BUTTON_1:
            Event->MouseButton.Button = RR_MOUSE_BUTTON_LEFT;
            break;
        case GLFW_MOUSE_BUTTON_2:
            Event->MouseButton.Button = RR_MOUSE_BUTTON_RIGHT;
            break;
        case GLFW_MOUSE_BUTTON_3:
            Event->MouseButton.Button = RR_MOUSE_BUTTON_MIDDLE;
            break;
        case GLFW_MOUSE_BUTTON_4:
            Event->MouseButton.Button = RR_MOUSE_BUTTON_X1;
            break;
        case GLFW_MOUSE_BUTTON_5:
            Event->MouseButton.Button = RR_MOUSE_BUTTON_X2;
            break;
        default:
            Event->MouseButton.Button = UINT8_MAX;
            break;
    }
    /* For each mouse button. */
    static uint64_t LastClickTime[5] = { 0 };
    static uint8_t Clicks[5] = { 0 };
    if (Action == GLFW_PRESS)
    {
        uint64_t Now = Rr_GetTimeMS();
        uint64_t Diff = Now - LastClickTime[Event->MouseButton.Button];
        if (Diff < RR_DOUBLE_CLICK_TIME_MS)
        {
            Clicks[Event->MouseButton.Button]++;
        }
        else
        {
            Clicks[Event->MouseButton.Button] = 0;
        }
        Event->MouseButton.Clicks = Clicks[Event->MouseButton.Button] + 1;
        LastClickTime[Event->MouseButton.Button] = Now;
    }
    else
    {
        Event->MouseButton.Clicks = 1;
    }
}

static inline void Rr_CodepointToUTF8(uint32_t Codepoint, char Buffer[5])
{
    if (Codepoint < 0x80)
    {
        Buffer[0] = (char)Codepoint;
        Buffer[1] = '\0';
    }
    else if (Codepoint < 0x800)
    {
        Buffer[0] = (char)(0xC0 | (Codepoint >> 6));
        Buffer[1] = (char)(0x80 | (Codepoint & 0x3f));
        Buffer[2] = '\0';
    }
    else if (Codepoint < 0x10000)
    {
        Buffer[0] = (char)(0xE0 | (Codepoint >> 12));
        Buffer[1] = (char)(0x80 | ((Codepoint >> 6) & 0x3f));
        Buffer[2] = (char)(0x80 | (Codepoint & 0x3f));
        Buffer[3] = '\0';
    }
    else if (Codepoint < 0x200000)
    {
        Buffer[0] = (char)(0xF0 | (Codepoint >> 18));
        Buffer[1] = (char)(0x80 | ((Codepoint >> 12) & 0x3f));
        Buffer[2] = (char)(0x80 | ((Codepoint >> 6) & 0x3f));
        Buffer[3] = (char)(0x80 | (Codepoint & 0x3f));
        Buffer[4] = '\0';
    }
}

static void Rr_GLFWCharCallback(GLFWwindow *Window, uint32_t Codepoint)
{
    Rr_Platform *RrWindow = glfwGetWindowUserPointer(Window);
    char *Buffer = RR_ALLOC_NO_ZERO(RrWindow->Arena, 5);
    Rr_CodepointToUTF8(Codepoint, Buffer);

    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_TEXT_INPUT;
    Event->Text.Text = Buffer;
}

static void Rr_GLFWWindowCloseCallback(GLFWwindow *Window)
{
    Rr_Event *Event = Rr_AddEvent();
    Event->Type = RR_EVENT_TYPE_QUIT;
}

static void Rr_GLFWWindowContentScaleCallback(
    GLFWwindow *Window,
    float X,
    float Y)
{
    gPlatform->WindowScale.X = X;
    gPlatform->WindowScale.Y = Y;
}

bool Rr_InitPlatformLibrary(Rr_AppConfig *Config)
{
    assert(gPlatform == NULL);

    bool WindowScaled = false;

#ifdef __linux__
    int32_t GLFWPlatform;
    // if (glfwPlatformSupported(GLFW_PLATFORM_WAYLAND))
    // {
    //     GLFWPlatform = GLFW_PLATFORM_WAYLAND;
    // }
    // else
    {
        GLFWPlatform = GLFW_PLATFORM_X11;
    }
    WindowScaled = GLFWPlatform == GLFW_PLATFORM_WAYLAND;
    glfwInitHint(GLFW_PLATFORM, GLFWPlatform);
#endif

    glfwInit();

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    /* NOTE: glfwGetWindowContentScale wouldn't return correct value if
     * GLFW_VISIBLE is set to false. */
    /* glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE); */
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(
        GLFW_RESIZABLE,
        (int)RR_HAS_BIT(Config->WindowFlags, RR_WINDOW_FLAGS_RESIZE_BIT));

    Rr_Arena *Arena = Rr_CreateDefaultArena();
    gPlatform = RR_ALLOC_TYPE(Arena, Rr_Platform);
    gPlatform->Arena = Arena;
    gPlatform->EventScratch =
        (Rr_Scratch){ .Arena = Arena, .Position = Arena->Position };
    Rr_IntVec2 WindowSize = Rr_GetDefaultWindowSize();
    gPlatform->Window = glfwCreateWindow(
        WindowSize.Width,
        WindowSize.Height,
        Config->Title,
        NULL,
        NULL);
    gPlatform->WindowScaled = WindowScaled;
    glfwGetWindowContentScale(
        gPlatform->Window,
        &gPlatform->WindowScale.X,
        &gPlatform->WindowScale.Y);

    glfwSetWindowUserPointer(gPlatform->Window, gPlatform);
    glfwSetCursorPosCallback(gPlatform->Window, &Rr_GLFWCursorCallback);
    glfwSetKeyCallback(gPlatform->Window, &Rr_GLFWKeyCallback);
    glfwSetScrollCallback(gPlatform->Window, &Rr_GLFWScrollCallback);
    glfwSetDropCallback(gPlatform->Window, &Rr_GLFWDropCallback);
    glfwSetWindowSizeCallback(gPlatform->Window, &Rr_GLFWWindowSizeCallback);
    glfwSetMouseButtonCallback(gPlatform->Window, &Rr_GLFWMouseButtonCallback);
    glfwSetCharCallback(gPlatform->Window, &Rr_GLFWCharCallback);
    glfwSetWindowCloseCallback(gPlatform->Window, &Rr_GLFWWindowCloseCallback);
    glfwSetWindowContentScaleCallback(
        gPlatform->Window,
        &Rr_GLFWWindowContentScaleCallback);

    if (gPlatform->WindowScaled)
    {
        Rr_SetWindowSize(WindowSize);
    }

    RR_LOG("Using GLFW platform library");

    return true;
}

bool Rr_CleanupPlatformLibrary(void)
{
    assert(gPlatform != NULL);

    glfwDestroyWindow(gPlatform->Window);
    Rr_DestroyArena(gPlatform->Arena);
    gPlatform = NULL;

    glfwTerminate();

    return true;
}

void (*Rr_GetVkGetInstanceProcAddr(void))(void)
{
    return (void (*)(void))&glfwGetInstanceProcAddress;
}

const char *const *Rr_GetVulkanExtensions(uint32_t *Count)
{
    return glfwGetRequiredInstanceExtensions(Count);
}

bool Rr_CreateVulkanSurface(void *Instance, void **Surface)
{
    return glfwCreateWindowSurface(
               (VkInstance)Instance,
               gPlatform->Window,
               NULL,
               (VkSurfaceKHR *)Surface) == VK_SUCCESS;
}

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return glfwGetKey(gPlatform->Window, (int)Scancode);
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    return Rr_GetGLFWCursorPos();
}

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    Rr_MouseButtonFlags Result = 0;
    if (glfwGetMouseButton(gPlatform->Window, GLFW_MOUSE_BUTTON_1))
    {
        Result |= RR_MOUSE_BUTTON_LEFT_BIT;
    }
    if (glfwGetMouseButton(gPlatform->Window, GLFW_MOUSE_BUTTON_2))
    {
        Result |= RR_MOUSE_BUTTON_RIGHT_BIT;
    }
    if (glfwGetMouseButton(gPlatform->Window, GLFW_MOUSE_BUTTON_3))
    {
        Result |= RR_MOUSE_BUTTON_MIDDLE_BIT;
    }
    if (glfwGetMouseButton(gPlatform->Window, GLFW_MOUSE_BUTTON_4))
    {
        Result |= RR_MOUSE_BUTTON_X1_BIT;
    }
    if (glfwGetMouseButton(gPlatform->Window, GLFW_MOUSE_BUTTON_5))
    {
        Result |= RR_MOUSE_BUTTON_X2_BIT;
    }
    return Result;
}

bool Rr_PollPlatformEvent(Rr_Event *Event)
{
    glfwPollEvents();
    return false;
}

void Rr_ShowWindow(void)
{
    glfwShowWindow(gPlatform->Window);
}

bool Rr_IsWindowMinimized(void)
{
    return false;
}

bool Rr_IsWindowFullscreen(void)
{
    return gPlatform->WindowedFullscreen;
}

void Rr_SetWindowFullscreen(bool Fullscreen)
{
    if (Fullscreen)
    {
        if (gPlatform->WindowedFullscreen == false)
        {
            glfwGetWindowPos(
                gPlatform->Window,
                &gPlatform->WindowedOffset.X,
                &gPlatform->WindowedOffset.Y);
            glfwGetWindowSize(
                gPlatform->Window,
                &gPlatform->WindowedExtent.X,
                &gPlatform->WindowedExtent.Y);
        }
        const GLFWvidmode *Mode = glfwGetVideoMode(Rr_GetGLFWMonitor());
        glfwSetWindowMonitor(
            gPlatform->Window,
            Rr_GetGLFWMonitor(),
            0,
            0,
            Mode->width,
            Mode->height,
            GLFW_DONT_CARE);
    }
    else
    {
        glfwSetWindowMonitor(
            gPlatform->Window,
            NULL,
            gPlatform->WindowedOffset.X,
            gPlatform->WindowedOffset.Y,
            gPlatform->WindowedExtent.Width,
            gPlatform->WindowedExtent.Height,
            GLFW_DONT_CARE);
    }
    gPlatform->WindowedFullscreen = Fullscreen;
}

void Rr_SetRelativeMouseMode(bool Relative)
{
    glfwSetInputMode(
        gPlatform->Window,
        GLFW_CURSOR,
        Relative ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
}

float Rr_GetDisplayRefreshRate(void)
{
    const GLFWvidmode *Mode = glfwGetVideoMode(Rr_GetGLFWMonitor());
    return (float)Mode->refreshRate;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    Rr_IntVec2 Size;
    glfwGetFramebufferSize(gPlatform->Window, &Size.X, &Size.Y);
    return Size;
}

void Rr_SetWindowTitle(const char *Title)
{
    glfwSetWindowTitle(gPlatform->Window, Title);
}

Rr_IntVec2 Rr_GetDisplaySize(void)
{
    Rr_IntVec2 Result;
    const GLFWvidmode *Mode = glfwGetVideoMode(Rr_GetGLFWMonitor());
    Result.Width = Mode->width;
    Result.Height = Mode->height;

    return Result;
}

void Rr_SetWindowSize(Rr_IntVec2 Size)
{
    if (gPlatform->WindowScaled)
    {
        glfwSetWindowSize(
            gPlatform->Window,
            (int32_t)((float)Size.X / gPlatform->WindowScale.X),
            (int32_t)((float)Size.Y / gPlatform->WindowScale.Y));
    }
    else
    {
        glfwSetWindowSize(gPlatform->Window, Size.X, Size.Y);
    }
}

void Rr_SetCursor(Rr_CursorType Type)
{
    switch (Type)
    {
        case RR_UI_CURSOR_TYPE_NORMAL:
        {
            static GLFWcursor *GLFWCursor;
            if (GLFWCursor == NULL)
            {
                GLFWCursor = glfwCreateStandardCursor(GLFW_ARROW_CURSOR);
            }
            glfwSetCursor(gPlatform->Window, GLFWCursor);
            return;
        }
        case RR_UI_CURSOR_TYPE_TEXT:
        {
            static GLFWcursor *GLFWCursor;
            if (GLFWCursor == NULL)
            {
                GLFWCursor = glfwCreateStandardCursor(GLFW_IBEAM_CURSOR);
            }
            glfwSetCursor(gPlatform->Window, GLFWCursor);
            return;
        }
        default:
            return;
    }
}

void Rr_SetClipboardText(const char *CString)
{
    glfwSetClipboardString(gPlatform->Window, CString);
}

const char *Rr_GetClipboardText(void)
{
    return glfwGetClipboardString(gPlatform->Window);
}
