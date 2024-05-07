#include "Rr_Input.h"

#include "Rr_Core.h"

#include <SDL3/SDL.h>

Rr_InputState UpdateKeyState(const Rr_KeyState OldKeyState, const u8* KeyboardState, const u8 Scancode)
{
    const bool bCurrentlyPressed = KeyboardState[Scancode] == 1;
    const bool bWasPressed = OldKeyState == RR_KEYSTATE_HELD || OldKeyState == RR_KEYSTATE_PRESSED;
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

void Rr_UpdateInputState(Rr_InputState* State, const Rr_InputConfig* Config)
{
    Rr_InputState NewState = *State;
    const u8* KeyboardState = SDL_GetKeyboardState(NULL);
    for (size_t Index = 0; Index < Config->Count; Index++)
    {
        const Rr_InputMapping* Mapping = &Config->Mappings[Index];

        const Rr_InputState OldKeyState = Rr_GetKeyState(NewState, Index);
        const Rr_InputState NewKeyState = UpdateKeyState(OldKeyState, KeyboardState, Mapping->Primary);
        NewState = NewState & ~(3 << (2 * Index));
        NewState = NewState | (NewKeyState << (2 * Index));
    }
    *State = NewState;
}

Rr_KeyState Rr_GetKeyState(const Rr_InputState State, const u32 Key)
{
    return (State >> (2 * Key)) & 3;
}
