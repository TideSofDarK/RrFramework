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
    u64 Last;
    u64 Ticks;
    f64 DeltaTime;
    f64 Seconds;
    f64 Acc;
    u64 Interval;
} SFrameTime;

typedef struct SRrApp
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    SRr Rr;
    SFrameTime FrameTime;
} SRrApp;

static u64 FrameTime_Advance(SFrameTime* FrameTime)
{
    u64 Now = SDL_GetTicksNS();
    u64 DeltaTimeNS = Now - FrameTime->Last;
    u64 DeltaTimeMS = DeltaTimeNS / 1000000;
    FrameTime->DeltaTime = (f64)DeltaTimeMS / 1000.0 * 1.0;
    FrameTime->Seconds += FrameTime->DeltaTime;
    FrameTime->Acc += FrameTime->DeltaTime;
    if (FrameTime->Acc >= 1.0)
    {
        FrameTime->Acc -= 1.0;

        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "ThreadID: %llu, Ticks: %llu, Time: %llums", (u64)SDL_GetCurrentThreadID(), FrameTime->Ticks, DeltaTimeMS);
        FrameTime->Ticks = 0;
    }
    FrameTime->Last = Now;
    FrameTime->Ticks++;
    return FrameTime->Interval - DeltaTimeNS;
}

static void ShowDebugOverlay()
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
    u64 ToSleepNS = FrameTime_Advance(&App->FrameTime);
    if (ToSleepNS > 0 && ToSleepNS < 10000000000)
    {
        SDL_DelayNS(ToSleepNS);
    }

    if (Rr_NewFrame(&App->Rr, App->Window))
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        igNewFrame();

        igShowDemoWindow(NULL);
        ShowDebugOverlay();

        igRender();

        Rr_Draw(&App->Rr);
    }
}

static int RrApp_Update(void* AppPtr)
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
                    return 0;
                }
                break;
                default:
                    break;
            }
        }

        RrApp_Iterate(App);
    }

    return 0;
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
        .FrameTime = { .Last = SDL_GetTicksNS(), .Interval = 16666666 }
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
