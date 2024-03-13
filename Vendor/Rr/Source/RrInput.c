#include "RrInput.h"

#include <SDL3/SDL.h>

#include "RrCore.h"
#include "RrTypes.h"

Rr_InputState UpdateKeyState(Rr_KeyState OldKeyState, const u8* KeyboardState, const u8 Scancode)
{
    b8 bCurrentlyPressed = KeyboardState[Scancode] == 1;
    b8 bWasPressed = OldKeyState == RR_KEYSTATE_HELD || OldKeyState == RR_KEYSTATE_PRESSED;
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

Rr_KeyState Rr_GetKeyState(Rr_InputState State, u32 Key)
{
    return (State >> (2 * Key)) & 3;
}

void Rr_UpdateInputState(Rr_InputState* State, Rr_InputConfig* Config)
{
    Rr_InputState NewState = *State;
    const u8* KeyboardState = SDL_GetKeyboardState(NULL);
    for (size_t Index = 0; Index < Config->Count; Index++)
    {
        Rr_InputMapping* Mapping = &Config->Mappings[Index];

        Rr_InputState OldKeyState = Rr_GetKeyState(NewState, Index);
        Rr_InputState NewKeyState = UpdateKeyState(OldKeyState, KeyboardState, Mapping->Primary);
        NewState = NewState & ~(3 << (2 * Index));
        NewState = NewState | (NewKeyState << (2 * Index));
    }
    *State = NewState;
}

