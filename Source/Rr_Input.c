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
