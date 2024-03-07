#include "RrApp.h"

#include "Rr/Rr.h"
#include "Rr/RrAsset.h"

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
    u64 Frames;
    f64 DeltaTime;
    f64 Seconds;
    f64 Acc;
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
    u64 Now = SDL_GetTicks();
    FrameTime->DeltaTime = (f64)(Now - FrameTime->Last) / 1000.0 * 1.0;
    FrameTime->Seconds += FrameTime->DeltaTime;
    FrameTime->Acc += FrameTime->DeltaTime;
    if (FrameTime->Acc >= 1.0)
    {
        FrameTime->Acc -= 1.0;

        SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "ThreadID: %llu, FPS: %llu", SDL_GetCurrentThreadID(), FrameTime->Frames);
        FrameTime->Frames = 0;
    }
    FrameTime->Last = Now;
    FrameTime->Frames++;
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
    FrameTime_Advance(&App->FrameTime);

    Rr_NewFrame(&App->Rr, App->Window);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();

    igShowDemoWindow(NULL);
    ShowDebugOverlay();

    igRender();

    Rr_Draw(&App->Rr);
}

static int RrApp_Update(void* AppPtr)
{
    SRrApp* App = (SRrApp*)AppPtr;

    while (SDL_AtomicGet(&App->bExit) == false)
    {
        for (SDL_Event Event; SDL_PollEvent(&Event);)
        {
            switch (Event.type)
            {
                case SDL_EVENT_QUIT:
                {
                    SDL_AtomicSet(&App->bExit, true);
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
    SRrApp* App = (SRrApp*)AppPtr;

    switch (Event->type)
    {
        case SDL_EVENT_WINDOW_EXPOSED:
        {
            SDL_AtomicSet(&App->Rr.Swapchain.bShouldResize, true);
            RrApp_Iterate(App);
        }
        break;
        default:
        {
            ImGui_ImplSDL3_ProcessEvent(Event);
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
        .FrameTime = { .Last = SDL_GetTicks() }
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
