#pragma once

#include "Rr_Core.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RR_MAX_INPUT_MAPPINGS 16

typedef struct Rr_App Rr_App;
typedef u32 Rr_InputState;

typedef enum Rr_KeyState
{
    RR_KEYSTATE_NONE,
    RR_KEYSTATE_PRESSED,
    RR_KEYSTATE_RELEASED,
    RR_KEYSTATE_HELD
} Rr_KeyState;

typedef struct Rr_InputMapping
{
    i32 Primary;
    i32 Secondary;
} Rr_InputMapping;

typedef struct Rr_InputConfig
{
    Rr_InputMapping* Mappings;
    size_t Count;
} Rr_InputConfig;

void Rr_UpdateInputState(Rr_InputState* State, const Rr_InputConfig* Config);
Rr_InputState Rr_GetInputState(Rr_App* App);
Rr_KeyState Rr_GetKeyState(Rr_InputState State, u32 Key);

#ifdef __cplusplus
}
#endif
