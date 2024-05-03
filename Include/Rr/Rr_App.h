#pragma once

#include <cglm/ivec2.h>

#include "Rr_Input.h"

typedef struct Rr_InputConfig Rr_InputConfig;
typedef struct Rr_Renderer Rr_Renderer;
typedef struct Rr_AppConfig Rr_AppConfig;
typedef struct Rr_App Rr_App;
typedef struct SDL_Window SDL_Window;

typedef struct Rr_AppConfig
{
    const char* Title;
    ivec2 ReferenceResolution;
    Rr_InputConfig* InputConfig;
    size_t PerFrameDataSize;
    void (*InitFunc)(Rr_App* App);
    void (*CleanupFunc)(Rr_App* App);
    void (*UpdateFunc)(Rr_App* App);
    void (*DrawFunc)(Rr_App* App);

    void (*FileDroppedFunc)(Rr_App* App, const char* Path);
} Rr_AppConfig;

void Rr_Run(Rr_AppConfig* Config);
void Rr_DebugOverlay(Rr_App* App);
void Rr_ToggleFullscreen(Rr_App* App);
Rr_Renderer* Rr_GetRenderer(Rr_App* App);
