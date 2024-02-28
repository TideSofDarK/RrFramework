#pragma once

#include "Core.h"
#include "Rr/Rr.h"

typedef struct SApp
{
    struct SDL_Window* Window;
    SRr Rr;
    b32 bExit;
} SApp;

void RunApp(void);
