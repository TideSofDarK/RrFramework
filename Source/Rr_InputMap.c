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

#include <Rr/Rr_InputMap.h>

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
    for(uint32_t Index = 0; Index < MappingCount; Index++)
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
