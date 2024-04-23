#pragma once

#include <cglm/ivec2.h>

#include "Rr_Input.h"
#include "Rr_Renderer.h"

typedef struct Rr_InputConfig Rr_InputConfig;
typedef struct SDL_Window SDL_Window;

typedef struct Rr_FrameTime
{
#ifdef RR_PERFORMANCE_COUNTER
    struct
    {
        f64 FPS;
        u64 Frames;
        u64 StartTime;
        u64 UpdateFrequency;
        f64 CountPerSecond;
    } PerformanceCounter;
#endif

    u64 TargetFramerate;
    u64 StartTime;
} Rr_FrameTime;

typedef struct Rr_App
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_Renderer Renderer;
    Rr_FrameTime FrameTime;
    Rr_AppConfig* Config;
} Rr_App;

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
