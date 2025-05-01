#include "Rr_App.h"

#include "Rr_Memory.h"

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_vulkan.h>

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

static void Rr_Iterate(Rr_App *App)
{
    Rr_CalculateDeltaTime(&App->FrameTime);

    Rr_PrepareFrame(App);

    Rr_BeginUI(App->UI);

    App->Config->IterateFunc(App, App->UserData);

    Rr_EndUI();

    bool Minimized = (SDL_GetWindowFlags(App->Window) & SDL_WINDOW_MINIMIZED);
    if(Minimized == false)
    {
        Rr_DrawFrame(App);
    }

#ifdef RR_PERFORMANCE_COUNTER
    Rr_CalculateFPS(&App->FrameTime);
#endif

    if(App->FrameTime.EnableFrameLimiter)
    {
        Rr_SimulateVSync(&App->FrameTime);
    }
}

static _Bool SDLCALL Rr_EventWatch(void *UserData, SDL_Event *Event)
{
    Rr_App *App = (Rr_App *)UserData;
    switch(Event->type)
    {
#ifdef SDL_PLATFORM_WIN32
        case SDL_EVENT_WINDOW_EXPOSED:
        {
            Rr_SetSwapchainDirty(Rr_GetRenderer(App), true);
            Rr_Iterate(App);
        }
        break;
#else
        case SDL_EVENT_WINDOW_RESIZED:
        {
            // Rr_SetSwapchainDirty(Rr_GetRenderer(App), true);
        }
        break;
#endif
        default:
        {
        }
        break;
    }

    return 0;
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
    Rr_InitPlatform();

    SDL_SetAppMetadata(Config->Title, Config->Version, Config->Package);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_CRITICAL);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_IntVec2 WindowSize = Rr_GetDefaultWindowSize();

    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_App *App = RR_ALLOC_TYPE(Arena, Rr_App);
    App->Arena = Arena;

    App->Config = Config;
    App->Window = SDL_CreateWindow(
        Config->Title,
        WindowSize.Width,
        WindowSize.Height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN |
            SDL_WINDOW_HIGH_PIXEL_DENSITY);
    App->SyncArena = Rr_CreateSyncArena();
    App->UserData = Config->UserData;

    Rr_SetScratchTLS(&App->ScratchArenaTLS);

    Rr_InitScratch(RR_MAIN_THREAD_SCRATCH_ARENA_SIZE);

    Rr_InitFrameTime(&App->FrameTime, App->Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_BUTTON_DOWN, true);
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, true);

    SDL_AddEventWatch(Rr_EventWatch, App);

    App->Renderer = Rr_CreateRenderer(App);
    Rr_RecreateSwapchain(App);
    App->UI = Rr_CreateUIContext(App);

    Config->InitFunc(App, App->UserData);

    SDL_ShowWindow(App->Window);

    while(Rr_GetAtomicInt(&App->QuitRequested) == false)
    {
        for(Rr_Event Event; Rr_PollEvent(&Event);)
        {
            Rr_ProcessUIEvent(App, &Event);

            switch(Event.Type)
            {
                case RR_EVENT_TYPE_QUIT:
                {
                    Rr_SetAtomicInt(&App->QuitRequested, true);
                }
                break;
                default:
                    break;
            }

            if(Config->EventFunc != NULL)
            {
                Config->EventFunc(App, &Event);
            }
        }

        Rr_Iterate(App);
    }

    Rr_WaitIdle(App->Renderer);

    App->Config->CleanupFunc(App, App->UserData);

    Rr_DestroyUIContext(App, App->UI);

    Rr_DestroyRenderer(App->Renderer);

    Rr_DestroySyncArena(&App->SyncArena);

    SDL_CleanupTLS();

    SDL_RemoveEventWatch((SDL_EventFilter)Rr_EventWatch, &App);

    SDL_DestroyWindow(App->Window);

    Rr_DestroyArena(App->Arena);

    SDL_Quit();
}

void Rr_SetFrameLimiterEnabled(Rr_App *App, bool Enabled)
{
    App->FrameTime.EnableFrameLimiter = Enabled;
}

Rr_Renderer *Rr_GetRenderer(Rr_App *App)
{
    return App->Renderer;
}

Rr_IntVec2 Rr_GetWindowSize(Rr_App *App)
{
    Rr_IntVec2 Size;
    SDL_GetWindowSizeInPixels(App->Window, &Size.X, &Size.Y);
    return Size;
}

void Rr_SetWindowTitle(Rr_App *App, const char *Title)
{
    SDL_SetWindowTitle(App->Window, Title);
}

double Rr_GetFramesPerSecond(Rr_App *App)
{
    return (float)App->FrameTime.PerformanceCounter.FPS;
}

static bool Rr_IsAnyFullscreen(SDL_Window *Window)
{
    return (SDL_GetWindowFlags(Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(Rr_App *App)
{
    SDL_SetWindowFullscreen(App->Window, !Rr_IsAnyFullscreen(App->Window));
}

float Rr_GetAspectRatio(Rr_App *App)
{
    Rr_Renderer *Renderer = App->Renderer;
    return (float)Renderer->Swapchain.Extent.width /
           (float)Renderer->Swapchain.Extent.height;
}

double Rr_GetDeltaSeconds(Rr_App *App)
{
    return App->FrameTime.DeltaSeconds;
}

double Rr_GetTimeSeconds(Rr_App *App)
{
    return (double)SDL_GetTicks() / 1000.0;
}

void Rr_SetRelativeMouseMode(Rr_App *App, bool IsRelative)
{
    SDL_SetWindowRelativeMouseMode(App->Window, IsRelative ? true : false);
}
