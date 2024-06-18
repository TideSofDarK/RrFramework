#pragma once

#include "Rr_Defines.h"
#include "Rr_Math.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RR_MAX_INPUT_MAPPINGS 16

typedef enum Rr_Scancode
{
    RR_SCANCODE_UNKNOWN = 0,
    RR_SCANCODE_A = 4,
    RR_SCANCODE_B = 5,
    RR_SCANCODE_C = 6,
    RR_SCANCODE_D = 7,
    RR_SCANCODE_E = 8,
    RR_SCANCODE_F = 9,
    RR_SCANCODE_G = 10,
    RR_SCANCODE_H = 11,
    RR_SCANCODE_I = 12,
    RR_SCANCODE_J = 13,
    RR_SCANCODE_K = 14,
    RR_SCANCODE_L = 15,
    RR_SCANCODE_M = 16,
    RR_SCANCODE_N = 17,
    RR_SCANCODE_O = 18,
    RR_SCANCODE_P = 19,
    RR_SCANCODE_Q = 20,
    RR_SCANCODE_R = 21,
    RR_SCANCODE_S = 22,
    RR_SCANCODE_T = 23,
    RR_SCANCODE_U = 24,
    RR_SCANCODE_V = 25,
    RR_SCANCODE_W = 26,
    RR_SCANCODE_X = 27,
    RR_SCANCODE_Y = 28,
    RR_SCANCODE_Z = 29,
    RR_SCANCODE_1 = 30,
    RR_SCANCODE_2 = 31,
    RR_SCANCODE_3 = 32,
    RR_SCANCODE_4 = 33,
    RR_SCANCODE_5 = 34,
    RR_SCANCODE_6 = 35,
    RR_SCANCODE_7 = 36,
    RR_SCANCODE_8 = 37,
    RR_SCANCODE_9 = 38,
    RR_SCANCODE_0 = 39,
    RR_SCANCODE_RETURN = 40,
    RR_SCANCODE_ESCAPE = 41,
    RR_SCANCODE_BACKSPACE = 42,
    RR_SCANCODE_TAB = 43,
    RR_SCANCODE_SPACE = 44,
    RR_SCANCODE_CAPSLOCK = 57,
    RR_SCANCODE_F1 = 58,
    RR_SCANCODE_F2 = 59,
    RR_SCANCODE_F3 = 60,
    RR_SCANCODE_F4 = 61,
    RR_SCANCODE_F5 = 62,
    RR_SCANCODE_F6 = 63,
    RR_SCANCODE_F7 = 64,
    RR_SCANCODE_F8 = 65,
    RR_SCANCODE_F9 = 66,
    RR_SCANCODE_F10 = 67,
    RR_SCANCODE_F11 = 68,
    RR_SCANCODE_F12 = 69,
    RR_SCANCODE_HOME = 74,
    RR_SCANCODE_PAGEUP = 75,
    RR_SCANCODE_DELETE = 76,
    RR_SCANCODE_END = 77,
    RR_SCANCODE_PAGEDOWN = 78,
    RR_SCANCODE_RIGHT = 79,
    RR_SCANCODE_LEFT = 80,
    RR_SCANCODE_DOWN = 81,
    RR_SCANCODE_UP = 82
} Rr_Scancode;

typedef enum Rr_MouseButton
{
    RR_MOUSE_BUTTON_LEFT = 1,
    RR_MOUSE_BUTTON_MIDDLE = 2,
    RR_MOUSE_BUTTON_RIGHT = 3,
    RR_MOUSE_BUTTON_X1 = 4,
    RR_MOUSE_BUTTON_X2 = 5
} Rr_MouseButton;

#define RR_MOUSE_BUTTON_MASK(X) (1u << ((X) - 1))

typedef enum Rr_MouseButtonMask
{
    RR_MOUSE_BUTTON_LEFT_MASK = RR_MOUSE_BUTTON_MASK(RR_MOUSE_BUTTON_LEFT),
    RR_MOUSE_BUTTON_MIDDLE_MASK = RR_MOUSE_BUTTON_MASK(RR_MOUSE_BUTTON_MIDDLE),
    RR_MOUSE_BUTTON_RIGHT_MASK = RR_MOUSE_BUTTON_MASK(RR_MOUSE_BUTTON_RIGHT),
    RR_MOUSE_BUTTON_X1_MASK = RR_MOUSE_BUTTON_MASK(RR_MOUSE_BUTTON_X1),
    RR_MOUSE_BUTTON_X2_MASK = RR_MOUSE_BUTTON_MASK(RR_MOUSE_BUTTON_X2)
} Rr_MouseButtonMask;

typedef Rr_U32 Rr_KeyStates;

typedef struct Rr_InputState Rr_InputState;
struct Rr_InputState
{
    Rr_KeyStates Keys;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MousePositionDelta;
    Rr_MouseButtonMask MouseState;
};

typedef enum Rr_KeyState
{
    RR_KEYSTATE_NONE,
    RR_KEYSTATE_PRESSED,
    RR_KEYSTATE_RELEASED,
    RR_KEYSTATE_HELD
} Rr_KeyState;

typedef struct Rr_InputMapping
{
    Rr_Scancode Primary;
    Rr_Scancode Secondary;
} Rr_InputMapping;

typedef struct Rr_InputConfig
{
    Rr_InputMapping* Mappings;
    size_t Count;
} Rr_InputConfig;

void Rr_UpdateInputState(Rr_InputState* State, Rr_InputConfig* Config);
Rr_KeyState Rr_GetKeyState(Rr_KeyStates Keys, Rr_U32 Key);

#ifdef __cplusplus
}
#endif
