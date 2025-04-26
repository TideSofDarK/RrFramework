#pragma once

#include <Rr/Rr_Input.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RR_MAX_INPUT_MAPPINGS 16

typedef uint32_t Rr_KeyStates;

typedef struct Rr_InputState Rr_InputState;
struct Rr_InputState
{
    Rr_KeyStates Keys;
    Rr_Vec2 MousePosition;
    Rr_Vec2 MousePositionDelta;
    Rr_MouseButtonFlags MouseState;
};

typedef enum Rr_KeyState
{
    RR_KEYSTATE_NONE,
    RR_KEYSTATE_PRESSED,
    RR_KEYSTATE_RELEASED,
    RR_KEYSTATE_HELD
} Rr_KeyState;

typedef struct Rr_InputMapping Rr_InputMapping;
struct Rr_InputMapping
{
    Rr_Scancode Primary;
    Rr_Scancode Secondary;
};

extern void Rr_UpdateInputState(
    size_t MappingCount,
    Rr_InputMapping *Mappings,
    Rr_InputState *State);

extern Rr_KeyState Rr_GetKeyState(Rr_KeyStates Keys, uint32_t Key);

#ifdef __cplusplus
}
#endif
