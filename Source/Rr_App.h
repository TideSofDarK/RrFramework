#pragma once

#include "Rr_Load.h"
#include "Rr_Object.h"
#include "Rr_Renderer.h"
#include <Rr/Rr_App.h>

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_video.h>

typedef struct Rr_FrameTime Rr_FrameTime;
struct Rr_FrameTime
{
#ifdef RR_PERFORMANCE_COUNTER
    struct
    {
        double FPS;
        uint64_t Frames;
        uint64_t StartTime;
        uint64_t UpdateFrequency;
        double CountPerSecond;
    } PerformanceCounter;
#endif

    /* Frame Limiter */
    uint64_t TargetFramerate;
    uint64_t StartTime;
    bool EnableFrameLimiter;

    /* Delta Time Calculation */
    uint64_t Last;
    uint64_t Now;
    double DeltaSeconds;
};

struct Rr_App
{
    Rr_Renderer Renderer;
    Rr_AppConfig *Config;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_FrameTime FrameTime;
    Rr_LoadingThread LoadingThread;
    Rr_Arena PermanentArena;
    Rr_SyncArena SyncArena;
    Rr_ObjectStorage ObjectStorage;
    SDL_TLSID ScratchArenaTLS;
    SDL_Window *Window;
    SDL_AtomicInt bExit;
    void *UserData;
};
