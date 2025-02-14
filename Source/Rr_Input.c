#include "Rr/Rr_Input.h"

#include <SDL3/SDL.h>

static Rr_KeyState Rr_UpdateKeyState(
    Rr_KeyState OldKeyState,
    const bool *KeyboardState,
    uint8_t Scancode)
{
    bool CurrentlyPressed = KeyboardState[Scancode] == 1;
    bool WasPressed =
        OldKeyState == RR_KEYSTATE_HELD || OldKeyState == RR_KEYSTATE_PRESSED;
    if(CurrentlyPressed)
    {
        if(WasPressed)
        {
            return RR_KEYSTATE_HELD;
        }
        return RR_KEYSTATE_PRESSED;
    }
    if(WasPressed)
    {
        return RR_KEYSTATE_RELEASED;
    }
    return RR_KEYSTATE_NONE;
}

void Rr_UpdateInputState(Rr_InputState *State, Rr_InputConfig *Config)
{
    Rr_KeyStates NewKeys = State->Keys;
    const bool *KeyboardState = SDL_GetKeyboardState(NULL);
    for(size_t Index = 0; Index < Config->Count; Index++)
    {
        Rr_InputMapping *Mapping = &Config->Mappings[Index];

        Rr_KeyState OldKeyState = Rr_GetKeyState(NewKeys, Index);
        Rr_KeyState NewKeyState =
            Rr_UpdateKeyState(OldKeyState, KeyboardState, Mapping->Primary);
        NewKeys = NewKeys & ~(3 << (2 * Index));
        NewKeys = NewKeys | (NewKeyState << (2 * Index));
    }
    State->Keys = NewKeys;

    SDL_GetRelativeMouseState(
        &State->MousePositionDelta.X,
        &State->MousePositionDelta.Y);
    State->MouseState =
        SDL_GetMouseState(&State->MousePosition.X, &State->MousePosition.Y);
}

Rr_KeyState Rr_GetKeyState(Rr_KeyStates Keys, uint32_t Key)
{
    return (Keys >> (2 * Key)) & 3;
}
