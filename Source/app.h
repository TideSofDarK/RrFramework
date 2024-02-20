#pragma once

#include "core.h"
#include "renderer.h"

typedef struct SApp
{
    struct SDL_Window* Window;
    SRenderer Renderer;
    bool bExit;
} SApp;

void CreateApp(SApp* App);

void RunApp(SApp* App);
