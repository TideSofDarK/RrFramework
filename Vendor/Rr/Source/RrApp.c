#include "RrApp.h"

#include "Rr/RrRenderer.h"
#include "Rr/RrAsset.h"
#include "RrTypes.h"
#include <SDL_timer.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_platform.h>

#define RR_PERFORMANCE_COUNTER 1

typedef struct SFrameTime
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
} SFrameTime;

typedef struct SRrApp
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    SRr Rr;
    SFrameTime FrameTime;
} SRrApp;

static void FrameTime_Advance(SFrameTime* FrameTime)
{
#ifdef RR_PERFORMANCE_COUNTER
    {
        FrameTime->PerformanceCounter.Frames++;
        u64 CurrentTime = SDL_GetPerformanceCounter();
        if (CurrentTime - FrameTime->PerformanceCounter.StartTime >= FrameTime->PerformanceCounter.UpdateFrequency)
        {
            f64 Elapsed = (f64)(CurrentTime - FrameTime->PerformanceCounter.StartTime) / FrameTime->PerformanceCounter.CountPerSecond;
            FrameTime->PerformanceCounter.FPS = (f64)FrameTime->PerformanceCounter.Frames / Elapsed;
            FrameTime->PerformanceCounter.StartTime = CurrentTime;
            FrameTime->PerformanceCounter.Frames = 0;
        }
    }
#endif

    u64 Interval = SDL_MS_TO_NS(1000) / FrameTime->TargetFramerate;
    u64 Now = SDL_GetTicksNS();
    u64 Elapsed = Now - FrameTime->StartTime;
    if (Elapsed < Interval)
    {
        SDL_DelayNS(Interval - Elapsed);
        Now = SDL_GetTicksNS();
    }

    Elapsed = Now - FrameTime->StartTime;

    if (!FrameTime->StartTime || Elapsed > SDL_MS_TO_NS(1000))
    {
        FrameTime->StartTime = Now;
    }
    else
    {
        FrameTime->StartTime += (Elapsed / Interval) * Interval;
    }
}

static void ShowDebugOverlay(SRrApp* App)
{
    ImGuiIO* IO = igGetIO();
    const ImGuiViewport* Viewport = igGetMainViewport();
    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const float Padding = 10.0f;
    ImVec2 WorkPos = Viewport->WorkPos;
    ImVec2 WindowPos, WindowPosPivot;
    WindowPos.x = WorkPos.x + Padding;
    WindowPos.y = WorkPos.y + Padding;
    WindowPosPivot.x = 0.0f;
    WindowPosPivot.y = 0.0f;
    igSetNextWindowPos(WindowPos, ImGuiCond_Always, WindowPosPivot);
    igSetNextWindowViewport(Viewport->ID);
    Flags |= ImGuiWindowFlags_NoMove;
    igSetNextWindowBgAlpha(0.35f);
    if (igBegin("Debug Overlay", NULL, Flags))
    {
        igText("SDL Allocations: %zu", SDL_GetNumAllocations());
#ifdef RR_PERFORMANCE_COUNTER
        igText("FPS: %.2f", App->FrameTime.PerformanceCounter.FPS);
#endif
        igSliderScalar("Target FPS", ImGuiDataType_U64, &App->FrameTime.TargetFramerate, &(u64){ 30 }, &(u64){ 480 }, "%d", ImGuiSliderFlags_None);
        igSeparator();
        if (igIsMousePosValid(NULL))
        {
            igText("Mouse Position: (%.1f,%.1f)", IO->MousePos.x, IO->MousePos.y);
        }
        else
        {
            igText("Mouse Position: <invalid>");
        }
    }
    igEnd();
}

static void RrApp_Iterate(SRrApp* App)
{
    if (Rr_NewFrame(&App->Rr, App->Window))
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        igNewFrame();

        igShowDemoWindow(NULL);
        ShowDebugOverlay(App);

        igRender();

        Rr_Draw(&App->Rr);
    }

    FrameTime_Advance(&App->FrameTime);
}

static void RrApp_Update(void* AppPtr)
{
    SRrApp* App = (SRrApp*)AppPtr;

    while (SDL_AtomicGet(&App->bExit) == false)
    {
        for (SDL_Event Event; SDL_PollEvent(&Event);)
        {
            ImGui_ImplSDL3_ProcessEvent(&Event);
            switch (Event.type)
            {
                case SDL_EVENT_QUIT:
                {
                    SDL_AtomicSet(&App->bExit, true);
                    return;
                }
                break;
                default:
                    break;
            }
        }

        RrApp_Iterate(App);
    }
}

static int SDLCALL RrApp_EventWatch(void* AppPtr, SDL_Event* Event)
{
    switch (Event->type)
    {
#ifdef SDL_PLATFORM_WIN32
        case SDL_EVENT_WINDOW_EXPOSED:
        {
            SRrApp* App = (SRrApp*)AppPtr;
            SDL_AtomicSet(&App->Rr.Swapchain.bResizePending, 1);
            RrApp_Iterate(App);
        }
        break;
#else
        case SDL_EVENT_WINDOW_RESIZED:
        {
            SRrApp* App = (SRrApp*)AppPtr;
            SDL_AtomicSet(&App->Rr.Swapchain.bResizePending, 1);
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

static void InitFrameTime(SFrameTime* const FrameTime, SDL_Window* Window)
{
#ifdef RR_PERFORMANCE_COUNTER
    FrameTime->PerformanceCounter.StartTime = SDL_GetPerformanceCounter();
    FrameTime->PerformanceCounter.UpdateFrequency = SDL_GetPerformanceFrequency() / 2;
    FrameTime->PerformanceCounter.CountPerSecond = (f64)SDL_GetPerformanceFrequency();
#endif

    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode* Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (u64)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
}

void RrApp_Run(SRrAppConfig* Config)
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#ifdef RR_DEBUG
    RrArray_Test();
#endif

    SDL_Vulkan_LoadLibrary(NULL);

    SRrApp App = {
        .Window = SDL_CreateWindow(
            Config->Title,
            1600,
            800,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN),
    };

    InitFrameTime(&App.FrameTime, App.Window);

    SDL_AddEventWatch(RrApp_EventWatch, &App);

    SRrAsset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    SRrRawMesh RawMesh = RrRawMesh_FromOBJAsset(&DoorFrameOBJ);

    Rr_Init(&App.Rr, App.Window);
    Rr_SetMesh(&App.Rr, &RawMesh);
    Rr_InitImGui(&App.Rr, App.Window);

    SDL_ShowWindow(App.Window);

    RrApp_Update(&App);

    RrRawMesh_Cleanup(&RawMesh);
    Rr_Cleanup(&App.Rr);

    SDL_DelEventWatch(RrApp_EventWatch, &App);
    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}
