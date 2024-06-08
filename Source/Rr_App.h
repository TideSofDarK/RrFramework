#pragma once

#include "Rr/Rr_App.h"
#include "Rr_Renderer.h"
#include "Rr_Object.h"
#include "Rr_Load.h"

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_video.h>

typedef struct Rr_FrameTime Rr_FrameTime;
struct Rr_FrameTime
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

    u64 Last;
    f32 DeltaSeconds;
};

struct Rr_App
{
    Rr_Renderer Renderer;
    Rr_AppConfig* Config;
    Rr_InputConfig InputConfig;
    Rr_InputState InputState;
    Rr_FrameTime FrameTime;
    Rr_LoadingThread LoadingThread;
    Rr_Arena PermanentArena;
    Rr_SyncArena SyncArena;
    Rr_ObjectStorage ObjectStorage;
    SDL_TLSID ScratchArenaTLS;
    SDL_Window* Window;
    SDL_AtomicInt bExit;
};
