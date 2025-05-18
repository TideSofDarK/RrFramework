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
