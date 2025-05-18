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

#include <Rr/Rr_Input.h>

#include "Rr_App.h"

#include <SDL3/SDL.h>

bool Rr_IsScancodePressed(Rr_Scancode Scancode)
{
    return SDL_GetKeyboardState(NULL)[Scancode];
}

Rr_Vec2 Rr_GetMousePosition(void)
{
    Rr_Vec2 MousePosition;
    SDL_GetMouseState(&MousePosition.X, &MousePosition.Y);

    Rr_IntVec2 WindowSize;
    SDL_GetWindowSize(gApp->Window, &WindowSize.X, &WindowSize.Y);

    Rr_IntVec2 WindowSizeInPixels;
    SDL_GetWindowSizeInPixels(
        gApp->Window,
        &WindowSizeInPixels.X,
        &WindowSizeInPixels.Y);

    MousePosition.X /= (float)WindowSize.X;
    MousePosition.Y /= (float)WindowSize.Y;

    MousePosition.X *= (float)WindowSizeInPixels.X;
    MousePosition.Y *= (float)WindowSizeInPixels.Y;

    return MousePosition;
}

Rr_Vec2 Rr_GetMousePositionDelta(void)
{
    Rr_Vec2 MousePositionDelta;
    SDL_GetRelativeMouseState(&MousePositionDelta.X, &MousePositionDelta.Y);
    return MousePositionDelta;
}

Rr_MouseButtonFlags Rr_GetMouseState(void)
{
    return SDL_GetMouseState(NULL, NULL);
}
