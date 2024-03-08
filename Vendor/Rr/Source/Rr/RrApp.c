#include "RrApp.h"

#include "Rr/Rr.h"
#include "Rr/RrAsset.h"
#include "RrTypes.h"

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

typedef struct SFrameTime
{
    f64 Acc;
    u64 FPSCounter;
    f64 TargetFramerate;
    f64 Clock;
    u64 Frames;
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
    f64 NewClock = (f64)SDL_GetTicks();
    f64 DeltaClock = (NewClock - FrameTime->Clock);
    f64 DeltaTicks = 1000.0 / FrameTime->TargetFramerate - DeltaClock;

    FrameTime->Acc += DeltaTicks / 1000.0;
    if (FrameTime->Acc >= 1.0)
    {
        FrameTime->Acc -= 1.0;

        FrameTime->FPSCounter = FrameTime->Frames;
        FrameTime->Frames = 0;
    }
    else
    {
        FrameTime->Frames++;
    }

    if (SDL_floor(DeltaTicks) > 0)
    {
        SDL_Delay((u32)DeltaTicks);
    }

    // FrameTime->Acc += (u64)DeltaTicks;
    // if (FrameTime->Acc >= 1000)
    // {
    //     FrameTime->Acc -= 1000;
    //
    //     SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "ThreadID: %llu, Ticks: %llu, Time: %llums", (u64)SDL_GetCurrentThreadID(), FrameTime->Ticks, DeltaTimeMS);
    //     FrameTime->Ticks = 0;
    // }

    if (DeltaTicks < -30)
    {
        FrameTime->Clock = NewClock - 30;
    }
    else
    {
        FrameTime->Clock = NewClock + DeltaTicks;
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
        igText("FPS: %d", App->FrameTime.FPSCounter);
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
        .FrameTime = { .TargetFramerate = 240 }
    };

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
