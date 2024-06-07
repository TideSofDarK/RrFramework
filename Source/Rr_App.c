#include "Rr_App.h"

#include "Rr_Load_Internal.h"
#include "Rr_Input.h"
#include "Rr_Memory.h"
#include "Rr_Types.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

static void FrameTime_Advance(Rr_FrameTime* FrameTime)
{
#ifdef RR_PERFORMANCE_COUNTER
    {
        FrameTime->PerformanceCounter.Frames++;
        const u64 CurrentTime = SDL_GetPerformanceCounter();
        if (CurrentTime - FrameTime->PerformanceCounter.StartTime >= FrameTime->PerformanceCounter.UpdateFrequency)
        {
            const f64 Elapsed = (f64)(CurrentTime - FrameTime->PerformanceCounter.StartTime) / FrameTime->PerformanceCounter.CountPerSecond;
            FrameTime->PerformanceCounter.FPS = (f64)FrameTime->PerformanceCounter.Frames / Elapsed;
            FrameTime->PerformanceCounter.StartTime = CurrentTime;
            FrameTime->PerformanceCounter.Frames = 0;
        }
    }
#endif

    const u64 Interval = SDL_MS_TO_NS(1000) / FrameTime->TargetFramerate;
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
    const ImGuiIO* IO = igGetIO();
    const ImGuiViewport* Viewport = igGetMainViewport();
    ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    const float Padding = 10.0f;
    const ImVec2 WorkPos = Viewport->WorkPos;
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
        igText("Swapchain Size: %dx%d", App->Renderer.SwapchainSize.width, App->Renderer.SwapchainSize.height);
        igText("SDL Allocations: %zu", SDL_GetNumAllocations());
        igText("RrFramework Objects: %zu", App->Renderer.ObjectStorage.ObjectCount);
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

static void Iterate(Rr_App* App)
{
    Rr_UpdateInputState(&App->InputState, &App->InputConfig);

    ImGui_ImplVulkan_NewFrame();
    ImGui_ImplSDL3_NewFrame();
    igNewFrame();

    Rr_ProcessPendingLoads(App);

    App->Config->IterateFunc(App);

    if (Rr_NewFrame(App, App->Window))
    {
        igRender();
        Rr_Draw(App);
    }

    FrameTime_Advance(&App->FrameTime);
}

static int SDLCALL Rr_EventWatch(void* AppPtr, SDL_Event* Event)
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

static void Rr_InitFrameTime(Rr_FrameTime* FrameTime, SDL_Window* Window)
{
#ifdef RR_PERFORMANCE_COUNTER
    FrameTime->PerformanceCounter.StartTime = SDL_GetPerformanceCounter();
    FrameTime->PerformanceCounter.UpdateFrequency = SDL_GetPerformanceFrequency() / 2;
    FrameTime->PerformanceCounter.CountPerSecond = (f64)SDL_GetPerformanceFrequency();
#endif

    const SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode* Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (u64)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
}

Rr_IntVec2 Rr_GetDefaultWindowSize()
{
    SDL_DisplayID DisplayID = SDL_GetPrimaryDisplay();

    SDL_Rect UsableBounds;
    SDL_GetDisplayUsableBounds(DisplayID, &UsableBounds);

    const f32 ScaleFactor = 0.75f;

    return (Rr_IntVec2){
        .Width = (i32)((f32)(UsableBounds.w - UsableBounds.x) * ScaleFactor),
        .Height = (i32)((f32)(UsableBounds.h - UsableBounds.y) * ScaleFactor)
    };
}

void Rr_Run(Rr_AppConfig* Config)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_IntVec2 WindowSize = Rr_GetDefaultWindowSize();

    Rr_App* App = Rr_StackAlloc(Rr_App, 1);
    SDL_zerop(App);
    *App = (Rr_App){
        .Config = Config,
        .Window = SDL_CreateWindow(
            Config->Title,
            WindowSize.Width,
            WindowSize.Height,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN),
        .InputConfig = {
            .Count = Config->InputConfig->Count,
            .Mappings = Config->InputConfig->Mappings },
        .PermanentArena = Rr_CreateArena(RR_PERMANENT_ARENA_SIZE),
        .SyncArena = Rr_CreateSyncArena(RR_SYNC_ARENA_SIZE),
        .ScratchArenaTLS = SDL_CreateTLS()
    };

    Rr_SetScratchTLS(&App->ScratchArenaTLS);

    Rr_InitThreadScratch(RR_SCRATCH_ARENA_SIZE);

    Rr_InitFrameTime(&App->FrameTime, App->Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    SDL_AddEventWatch(Rr_EventWatch, App);

    Rr_InitRenderer(App);
    Rr_InitLoadingThread(App);
    Rr_InitImGui(App);

    Config->InitFunc(App);

    SDL_ShowWindow(App->Window);

    while (SDL_AtomicGet(&App->bExit) == false)
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
                        Config->FileDroppedFunc(App, Event.drop.data);
                    }
                    break;
                }
                case SDL_EVENT_QUIT:
                {
                    SDL_AtomicSet(&App->bExit, true);
                    break;
                }
                default:
                    break;
            }
        }

        Iterate(App);
    }

    Rr_CleanupLoadingThread(App);
    Rr_CleanupRenderer(App);

    Rr_DestroyArena(&App->PermanentArena);
    Rr_DestroySyncArena(&App->SyncArena);

    SDL_CleanupTLS();

    SDL_DelEventWatch((SDL_EventFilter)Rr_EventWatch, App);
    SDL_DestroyWindow(App->Window);

    SDL_Quit();
}

static bool Rr_IsAnyFullscreen(SDL_Window* Window)
{
    return (SDL_GetWindowFlags(Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(Rr_App* App)
{
    SDL_SetWindowFullscreen(App->Window, !Rr_IsAnyFullscreen(App->Window));
}

Rr_InputState Rr_GetInputState(Rr_App* App)
{
    return App->InputState;
}

f32 Rr_GetAspectRatio(Rr_App* App)
{
    Rr_Renderer* Renderer = &App->Renderer;
    return (float)Renderer->SwapchainSize.width / (float)Renderer->SwapchainSize.height;
}
