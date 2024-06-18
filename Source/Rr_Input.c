#include "Rr/Rr_Input.h"

#include "Rr/Rr_Defines.h"

#include <SDL3/SDL.h>

static Rr_KeyState
Rr_UpdateKeyState(Rr_KeyState OldKeyState, const Rr_U8* KeyboardState, Rr_U8 Scancode)
{
    Rr_Bool bCurrentlyPressed = KeyboardState[Scancode] == 1;
    Rr_Bool bWasPressed = OldKeyState == RR_KEYSTATE_HELD || OldKeyState == RR_KEYSTATE_PRESSED;
    if (bCurrentlyPressed)
    {
        if (bWasPressed)
        {
            return RR_KEYSTATE_HELD;
        }
        else
        {
            return RR_KEYSTATE_PRESSED;
        }
    }
    else
    {
        if (bWasPressed)
        {
            return RR_KEYSTATE_RELEASED;
        }
        else
        {
            return RR_KEYSTATE_NONE;
        }
    }
}

void
Rr_UpdateInputState(Rr_InputState* State, Rr_InputConfig* Config)
{
    Rr_KeyStates NewKeys = State->Keys;
    const Rr_U8* KeyboardState = SDL_GetKeyboardState(NULL);
    for (Rr_USize Index = 0; Index < Config->Count; Index++)
    {
        Rr_InputMapping* Mapping = &Config->Mappings[Index];

        Rr_KeyState OldKeyState = Rr_GetKeyState(NewKeys, Index);
        Rr_KeyState NewKeyState = Rr_UpdateKeyState(OldKeyState, KeyboardState, Mapping->Primary);
        NewKeys = NewKeys & ~(3 << (2 * Index));
        NewKeys = NewKeys | (NewKeyState << (2 * Index));
    }
    State->Keys = NewKeys;

    SDL_GetRelativeMouseState(&State->MousePositionDelta.X, &State->MousePositionDelta.Y);
    State->MouseState = SDL_GetMouseState(&State->MousePosition.X, &State->MousePosition.Y);
}

Rr_KeyState
Rr_GetKeyState(Rr_KeyStates Keys, Rr_U32 Key)
{
    return (Keys >> (2 * Key)) & 3;
}
