#include <Rr/Rr_Input.h>

#include <SDL3/SDL.h>

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
