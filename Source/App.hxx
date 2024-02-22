#pragma once

#include "Core.hxx"
#include "Renderer.hxx"

typedef struct SApp
{
    struct SDL_Window* Window;
    SRenderer Renderer;
    b32 bExit;
} SApp;

extern "C" void RunApp();
