#pragma once

#include "Rr/Rr_App.h"
#include "Rr_Load.h"
#include "Rr_Object.h"
#include "Rr_Renderer.h"

#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_video.h>

typedef struct Rr_FrameTime Rr_FrameTime;
struct Rr_FrameTime
{
#ifdef RR_PERFORMANCE_COUNTER
    struct
    {
        Rr_F64 FPS;
        Rr_U64 Frames;
        Rr_U64 StartTime;
        Rr_U64 UpdateFrequency;
        Rr_F64 CountPerSecond;
    } PerformanceCounter;
#endif

    /* VSync Simulation */
    Rr_U64 TargetFramerate;
    Rr_U64 StartTime;
    Rr_Bool bSimulateVSync;

    /* Delta Time Calculation */
    Rr_U64 Last;
    Rr_U64 Now;
    Rr_F64 DeltaSeconds;
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
