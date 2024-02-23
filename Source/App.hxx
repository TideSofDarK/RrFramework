#pragma once

#include "Core.hxx"
#include "Rr.hxx"

typedef struct SApp
{
    struct SDL_Window* Window;
    SRr Rr;
    b32 bExit;
} SApp;

extern "C" void RunApp();
