#include "Rr_App.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_vulkan.h>

#include <assert.h>

Rr_App *gApp = NULL;

static void Rr_CalculateDeltaTime(Rr_FrameTime *FrameTime)
{
    FrameTime->Last = FrameTime->Now;
    FrameTime->Now = SDL_GetPerformanceCounter();
    FrameTime->DeltaSeconds = (double)(FrameTime->Now - FrameTime->Last) /
                              (double)SDL_GetPerformanceFrequency();
}

static void Rr_CalculateFPS(Rr_FrameTime *FrameTime)
{
    FrameTime->PerformanceCounter.Frames++;
    uint64_t CurrentTime = SDL_GetPerformanceCounter();
    if(CurrentTime - FrameTime->PerformanceCounter.StartTime >=
       FrameTime->PerformanceCounter.UpdateFrequency)
    {
        double Elapsed =
            (double)(CurrentTime - FrameTime->PerformanceCounter.StartTime) /
            FrameTime->PerformanceCounter.CountPerSecond;
        FrameTime->PerformanceCounter.FPS =
            (double)FrameTime->PerformanceCounter.Frames / Elapsed;
        FrameTime->PerformanceCounter.StartTime = CurrentTime;
        FrameTime->PerformanceCounter.Frames = 0;
    }
}

static void Rr_SimulateVSync(Rr_FrameTime *FrameTime)
{
    uint64_t Interval = SDL_MS_TO_NS(1000) / FrameTime->TargetFramerate;
    uint64_t Now = SDL_GetTicksNS();
    uint64_t Elapsed = Now - FrameTime->StartTime;

    if(Elapsed < Interval)
    {
        SDL_DelayNS(Interval - Elapsed);
        Now = SDL_GetTicksNS();
    }

    Elapsed = Now - FrameTime->StartTime;

    if(!FrameTime->StartTime || Elapsed > SDL_MS_TO_NS(1000))
    {
        FrameTime->StartTime = Now;
    }
    else
    {
        FrameTime->StartTime += (Elapsed / Interval) * Interval;
    }
}

static void Rr_Iterate(void)
{
    Rr_CalculateDeltaTime(&gApp->FrameTime);

    Rr_NewFrame();

    Rr_BeginUI(gApp->UI);

    gApp->Config->IterateFunc(gApp->UserData);

    Rr_EndUI();

    bool Minimized = (SDL_GetWindowFlags(gApp->Window) & SDL_WINDOW_MINIMIZED);
    if(Minimized == true)
    {
        SDL_Delay(100);
    }
    Rr_DrawFrame();

#ifdef RR_PERFORMANCE_COUNTER
    Rr_CalculateFPS(&gApp->FrameTime);
#endif

    if(gApp->FrameTime.EnableFrameLimiter)
    {
        Rr_SimulateVSync(&gApp->FrameTime);
    }
}

static void Rr_InitFrameTime(Rr_FrameTime *FrameTime, SDL_Window *Window)
{
#ifdef RR_PERFORMANCE_COUNTER
    FrameTime->PerformanceCounter.StartTime = SDL_GetPerformanceCounter();
    FrameTime->PerformanceCounter.UpdateFrequency =
        SDL_GetPerformanceFrequency() / 2;
    FrameTime->PerformanceCounter.CountPerSecond =
        (double)SDL_GetPerformanceFrequency();
#endif

    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (uint64_t)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
    FrameTime->Now = SDL_GetPerformanceCounter();
}

Rr_IntVec2 Rr_GetDefaultWindowSize(void)
{
    SDL_DisplayID DisplayID = SDL_GetPrimaryDisplay();

    SDL_Rect UsableBounds;
    SDL_GetDisplayUsableBounds(DisplayID, &UsableBounds);

    float ScaleFactor = 0.75f;

    return (Rr_IntVec2){
        .Width =
            (int32_t)((float)(UsableBounds.w - UsableBounds.x) * ScaleFactor),
        .Height =
            (int32_t)((float)(UsableBounds.h - UsableBounds.y) * ScaleFactor)
    };
}

void Rr_Run(Rr_AppConfig *Config)
{
    assert(gApp == NULL && "You shouldn't call Rr_Run() more than once!");

    Rr_InitPlatform();

    SDL_SetAppMetadata(Config->Title, Config->Version, Config->Package);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_IntVec2 WindowSize = Rr_GetDefaultWindowSize();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    gApp = RR_ALLOC_TYPE(Arena, Rr_App);
    gApp->Arena = Arena;

    gApp->Config = Config;
    gApp->Window = SDL_CreateWindow(
        Config->Title,
        WindowSize.Width,
        WindowSize.Height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
            SDL_WINDOW_HIGH_PIXEL_DENSITY);
    gApp->SyncArena = Rr_CreateSyncArena();
    gApp->UserData = Config->UserData;

    Rr_SetScratchTLS(&gApp->ScratchArenaTLS);

    Rr_InitScratch(RR_MAIN_THREAD_SCRATCH_ARENA_SIZE);

    Rr_InitFrameTime(&gApp->FrameTime, gApp->Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    gApp->Renderer = Rr_CreateRenderer();
    gApp->UI = Rr_CreateUIContext();

    Config->InitFunc(gApp->UserData);

    SDL_ShowWindow(gApp->Window);

    while(Rr_GetAtomicInt(&gApp->QuitRequested) == false)
    {
        for(Rr_Event Event; Rr_PollEvent(&Event);)
        {
            Rr_ProcessUIEvent(&Event);

            switch(Event.Type)
            {
                case RR_EVENT_TYPE_QUIT:
                {
                    Rr_SetAtomicInt(&gApp->QuitRequested, true);
                    break;
                }
                break;
                default:
                    break;
            }

            if(Config->EventFunc != NULL)
            {
                Config->EventFunc(&Event);
            }
        }

        Rr_Iterate();
    }

    Rr_WaitIdle(gApp->Renderer);

    gApp->Config->CleanupFunc(gApp->UserData);

    Rr_DestroyUIContext(gApp->UI);

    Rr_DestroyRenderer(gApp->Renderer);

    Rr_DestroySyncArena(&gApp->SyncArena);

    SDL_CleanupTLS();

    SDL_DestroyWindow(gApp->Window);

    Rr_DestroyArena(gApp->Arena);

    SDL_Quit();
}

void Rr_SetFrameLimiterEnabled(bool Enabled)
{
    gApp->FrameTime.EnableFrameLimiter = Enabled;
}

Rr_Renderer *Rr_GetRenderer(void)
{
    return gApp->Renderer;
}

Rr_IntVec2 Rr_GetWindowSize(void)
{
    Rr_IntVec2 Size;
    SDL_GetWindowSizeInPixels(gApp->Window, &Size.X, &Size.Y);
    return Size;
}

void Rr_SetWindowTitle(const char *Title)
{
    SDL_SetWindowTitle(gApp->Window, Title);
}

double Rr_GetFramesPerSecond(void)
{
    return (float)gApp->FrameTime.PerformanceCounter.FPS;
}

static bool Rr_IsAnyFullscreen(void)
{
    return (SDL_GetWindowFlags(gApp->Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(void)
{
    SDL_SetWindowFullscreen(gApp->Window, !Rr_IsAnyFullscreen());
}

float Rr_GetAspectRatio(void)
{
    Rr_Renderer *Renderer = gApp->Renderer;
    return (float)Renderer->Swapchain.Extent.width /
           (float)Renderer->Swapchain.Extent.height;
}

double Rr_GetDeltaSeconds(void)
{
    return gApp->FrameTime.DeltaSeconds;
}

double Rr_GetTimeSeconds(void)
{
    return (double)SDL_GetTicks() / 1000.0;
}

void Rr_SetRelativeMouseMode(bool IsRelative)
{
    SDL_SetWindowRelativeMouseMode(gApp->Window, IsRelative ? true : false);
}
