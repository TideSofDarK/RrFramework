#pragma once

#include <SDL3/SDL_atomic.h>

#include "Rr_Framework.h"
#include "Rr_Input.h"

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

struct Rr_App
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_Renderer* Renderer;
    Rr_FrameTime FrameTime;
    Rr_AppConfig* Config;
};
