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

void Rr_UpdateInputState(
    size_t MappingCount,
    Rr_InputMapping *Mappings,
    Rr_InputState *State)
{
    Rr_KeyStates NewKeys = State->Keys;
    const bool *KeyboardState = SDL_GetKeyboardState(NULL);
    for(size_t Index = 0; Index < MappingCount; Index++)
    {
        Rr_InputMapping *Mapping = &Mappings[Index];

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

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return SDL_GetKeyboardState(NULL)[Scancode];
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    Rr_Vec2 MousePosition;
    SDL_GetMouseState(&MousePosition.X, &MousePosition.Y);
    return MousePosition;
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    Rr_Vec2 MousePositionDelta;
    SDL_GetRelativeMouseState(&MousePositionDelta.X, &MousePositionDelta.Y);
    return MousePositionDelta;
}

Rr_MouseButtonMask Rr_GetMouseState(void)
{
    return SDL_GetMouseState(NULL, NULL);
}
