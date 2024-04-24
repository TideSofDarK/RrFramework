#include "Rr_App.h"

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

#include "Rr_Renderer.h"
#include "Rr_Asset.h"
#include "Rr_Input.h"
#include "Rr_Mesh.h"

static void FrameTime_Advance(Rr_FrameTime* FrameTime)
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

void Rr_DebugOverlay(Rr_App* App)
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
        igText("Reference Resolution: %dx%d", App->Renderer.DrawTarget.ReferenceResolution.width, App->Renderer.DrawTarget.ReferenceResolution.height);
        igText("Active Resolution: %dx%d", App->Renderer.DrawTarget.ActiveResolution.width, App->Renderer.DrawTarget.ActiveResolution.height);
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

static b8 BeginIterate(Rr_App* App)
{
    if (Rr_NewFrame(&App->Renderer, App->Window))
    {
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        igNewFrame();

        return true;
    }
    return false;
}

static void EndIterate(Rr_App* App)
{
    igRender();
    Rr_Draw(App);

    FrameTime_Advance(&App->FrameTime);
}

static void Iterate(Rr_App* App)
{
    if (BeginIterate(App))
    {
        Rr_UpdateInputState(&App->InputState, &App->InputConfig);
        App->Config->UpdateFunc(App);
        EndIterate(App);
    }
}

static int SDLCALL EventWatch(void* AppPtr, SDL_Event* Event)
{
    switch (Event->type)
    {
#ifdef SDL_PLATFORM_WIN32
        case SDL_EVENT_WINDOW_EXPOSED:
        {
            Rr_App* App = (Rr_App*)AppPtr;
            SDL_AtomicSet(&App->Renderer.Swapchain.bResizePending, 1);
            Iterate(App);
        }
        break;
#else
        case SDL_EVENT_WINDOW_RESIZED:
        {
            Rr_App* App = (Rr_App*)AppPtr;
            SDL_AtomicSet(&App->Renderer.Swapchain.bResizePending, 1);
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

static void InitFrameTime(Rr_FrameTime* const FrameTime, SDL_Window* Window)
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

void Rr_Run(Rr_AppConfig* Config)
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#ifdef RR_DEBUG
    Rr_Array_Test();
#endif

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_App App = {
        .Config = Config,
        .Window = SDL_CreateWindow(
            Config->Title,
            Config->ReferenceResolution[0],
            Config->ReferenceResolution[1],
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN),
        .InputConfig = {
            .Count = Config->InputConfig->Count,
            .Mappings = Config->InputConfig->Mappings }
    };

    InitFrameTime(&App.FrameTime, App.Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    SDL_AddEventWatch(EventWatch, &App);

    Rr_InitRenderer(&App);
    Rr_InitImGui(&App);

    Config->InitFunc(&App);

    SDL_ShowWindow(App.Window);

    while (SDL_AtomicGet(&App.bExit) == false)
    {
        for (SDL_Event Event; SDL_PollEvent(&Event);)
        {
            ImGui_ImplSDL3_ProcessEvent(&Event);
            switch (Event.type)
            {
                case SDL_EVENT_DROP_FILE:
                {
                    if (Config->FileDroppedFunc != NULL)
                    {
                        Config->FileDroppedFunc(&App, Event.drop.data);
                    }
                    break;
                }
                case SDL_EVENT_QUIT:
                {
                    SDL_AtomicSet(&App.bExit, true);
                    break;
                }
                break;
                default:
                    break;
            }
        }

        Iterate(&App);
    }

    Rr_CleanupRenderer(&App);

    SDL_DelEventWatch(EventWatch, &App);
    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}

static b8 Rr_IsAnyFullscreen(SDL_Window* Window)
{
    return (SDL_GetWindowFlags(Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(Rr_App* App)
{
    SDL_SetWindowFullscreen(App->Window, !Rr_IsAnyFullscreen(App->Window));
}
