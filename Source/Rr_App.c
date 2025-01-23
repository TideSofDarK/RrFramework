#include "Rr_App.h"

#include "Rr_Load.h"
#include "Rr_Memory.h"
#include "Rr_Mesh.h"
#include "Rr_UI.h"

#include <Rr/Rr_Input.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_vulkan.h>

union Rr_Object
{
    Rr_Buffer Buffer;
    Rr_Primitive Primitive;
    Rr_StaticMesh StaticMesh;
    Rr_SkeletalMesh SkeletalMesh;
    Rr_Image Image;
    Rr_Font Font;
    Rr_PipelineLayout PipelineLayout;
    Rr_GraphicsPipeline GraphicsPipeline;
    Rr_DrawTarget DrawTarget;
    void *Next;
};

static void Rr_CalculateDeltaTime(Rr_FrameTime *FrameTime)
{
    FrameTime->Last = FrameTime->Now;
    FrameTime->Now = SDL_GetPerformanceCounter();
    FrameTime->DeltaSeconds =
        (double)(FrameTime->Now - FrameTime->Last) * 1000.0 / (double)SDL_GetPerformanceFrequency();
}

static void Rr_CalculateFPS(Rr_FrameTime *FrameTime)
{
    FrameTime->PerformanceCounter.Frames++;
    uint64_t CurrentTime = SDL_GetPerformanceCounter();
    if(CurrentTime - FrameTime->PerformanceCounter.StartTime >= FrameTime->PerformanceCounter.UpdateFrequency)
    {
        double Elapsed = (double)(CurrentTime - FrameTime->PerformanceCounter.StartTime) /
                         FrameTime->PerformanceCounter.CountPerSecond;
        FrameTime->PerformanceCounter.FPS = (double)FrameTime->PerformanceCounter.Frames / Elapsed;
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

// void Rr_DebugOverlay(Rr_App *App)
// {
//     ImGuiIO *IO = igGetIO();
//     ImGuiViewport *Viewport = igGetMainViewport();
//     ImGuiWindowFlags Flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
//                              ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing |
//                              ImGuiWindowFlags_NoNav;
//     float Padding = 10.0f;
//     ImVec2 WorkPos = Viewport->WorkPos;
//     ImVec2 WindowPos, WindowPosPivot;
//     WindowPos.x = WorkPos.x + Padding;
//     WindowPos.y = WorkPos.y + Padding;
//     WindowPosPivot.x = 0.0f;
//     WindowPosPivot.y = 0.0f;
//     igSetNextWindowPos(WindowPos, ImGuiCond_Always, WindowPosPivot);
//     Flags |= ImGuiWindowFlags_NoMove;
//     igSetNextWindowBgAlpha(0.95f);
//     if(igBegin("Debug Overlay", NULL, Flags))
//     {
//         igText("Swapchain Size: %dx%d", App->Renderer.SwapchainSize.width, App->Renderer.SwapchainSize.height);
//         igText("SDL Allocations: %zu", SDL_GetNumAllocations());
//         igText("RrFramework Objects: %zu", App->ObjectStorage.ObjectCount);
//         igSeparator();
// #ifdef RR_PERFORMANCE_COUNTER
//         igText("FPS: %.2f", App->FrameTime.PerformanceCounter.FPS);
// #endif
//         igCheckbox("Simulate VSync", (_Bool *)&App->FrameTime.EnableFrameLimiter);
//         if(App->FrameTime.EnableFrameLimiter)
//         {
//             igSliderScalar(
//                 "Target FPS",
//                 ImGuiDataType_U64,
//                 &App->FrameTime.TargetFramerate,
//                 &(uint64_t){ 30 },
//                 &(uint64_t){ 480 },
//                 "%d",
//                 ImGuiSliderFlags_None);
//         }
//         igSeparator();
//         if(igIsMousePosValid(NULL))
//         {
//             igText("Mouse Position: (%.1f,%.1f)", IO->MousePos.x, IO->MousePos.y);
//         }
//         else
//         {
//             igText("Mouse Position: <invalid>");
//         }
//     }
//     igEnd();
// }

static void Iterate(Rr_App *App)
{
    Rr_CalculateDeltaTime(&App->FrameTime);

    Rr_UpdateInputState(&App->InputState, &App->InputConfig);

    Rr_PrepareFrame(App);
    Rr_BeginUI(App, App->UI);

    App->Config->IterateFunc(App, App->UserData);

    if(Rr_NewFrame(App, App->Window))
    {
        Rr_Draw(App);
    }

#ifdef RR_PERFORMANCE_COUNTER
    Rr_CalculateFPS(&App->FrameTime);
#endif

    if(App->FrameTime.EnableFrameLimiter)
    {
        Rr_SimulateVSync(&App->FrameTime);
    }
}

static _Bool SDLCALL Rr_EventWatch(void *AppPtr, SDL_Event *Event)
{
    switch(Event->type)
    {
#ifdef SDL_PLATFORM_WIN32
        case SDL_EVENT_WINDOW_EXPOSED:
        {
            Rr_App *App = (Rr_App *)AppPtr;
            SDL_SetAtomicInt(&App->Renderer.Swapchain.bResizePending, 1);
            Iterate(App);
        }
        break;
#else
        case SDL_EVENT_WINDOW_RESIZED:
        {
            Rr_App *App = (Rr_App *)AppPtr;
            SDL_SetAtomicInt(&App->Renderer.Swapchain.bResizePending, 1);
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
    FrameTime->PerformanceCounter.UpdateFrequency = SDL_GetPerformanceFrequency() / 2;
    FrameTime->PerformanceCounter.CountPerSecond = (double)SDL_GetPerformanceFrequency();
#endif

    SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode *Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (uint64_t)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
    FrameTime->EnableFrameLimiter = true;

    FrameTime->Now = SDL_GetPerformanceCounter();
}

Rr_IntVec2 Rr_GetDefaultWindowSize(void)
{
    SDL_DisplayID DisplayID = SDL_GetPrimaryDisplay();

    SDL_Rect UsableBounds;
    SDL_GetDisplayUsableBounds(DisplayID, &UsableBounds);

    float ScaleFactor = 0.75f;

    return (Rr_IntVec2){ .Width = (int32_t)((float)(UsableBounds.w - UsableBounds.x) * ScaleFactor),
                         .Height = (int32_t)((float)(UsableBounds.h - UsableBounds.y) * ScaleFactor) };
}

void Rr_Run(Rr_AppConfig *Config)
{
    Rr_InitPlatform();

    SDL_SetAppMetadata(Config->Title, Config->Version, Config->Package);
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_IntVec2 WindowSize = Rr_GetDefaultWindowSize();

    Rr_App App = (Rr_App){ 0 };
    App.Config = Config;
    App.Window = SDL_CreateWindow(
        Config->Title,
        WindowSize.Width,
        WindowSize.Height,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY);
    App.PermanentArena = Rr_CreateDefaultArena();
    App.SyncArena = Rr_CreateSyncArena();
    App.UserData = Config->UserData;

    Rr_SetScratchTLS(&App.ScratchArenaTLS);

    Rr_InitThreadScratch(RR_MAIN_THREAD_SCRATCH_ARENA_SIZE);

    Rr_InitFrameTime(&App.FrameTime, App.Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_BUTTON_DOWN, true);
    SDL_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, true);

    SDL_AddEventWatch(Rr_EventWatch, &App);

    Rr_InitRenderer(&App);
    Rr_InitLoadingThread(&App);
    App.UI = Rr_CreateUI(&App);

    Config->InitFunc(&App, App.UserData);

    SDL_ShowWindow(App.Window);

    while(SDL_GetAtomicInt(&App.bExit) == false)
    {
        for(SDL_Event Event; SDL_PollEvent(&Event);)
        {
            switch(Event.type)
            {
                case SDL_EVENT_DROP_FILE:
                {
                    if(Config->FileDroppedFunc != NULL)
                    {
                        Config->FileDroppedFunc(&App, Event.drop.data);
                    }
                    break;
                }
                case SDL_EVENT_QUIT:
                {
                    SDL_SetAtomicInt(&App.bExit, true);
                    break;
                }
                default:
                    break;
            }
        }

        Iterate(&App);
    }

    Rr_CleanupLoadingThread(&App);
    Rr_DestroyUI(&App, App.UI);
    Rr_CleanupRenderer(&App);

    Rr_DestroyArena(App.PermanentArena);
    Rr_DestroySyncArena(&App.SyncArena);

    SDL_CleanupTLS();

    SDL_RemoveEventWatch((SDL_EventFilter)Rr_EventWatch, &App);
    SDL_DestroyWindow(App.Window);

    SDL_Quit();
}

static bool Rr_IsAnyFullscreen(SDL_Window *Window)
{
    return (SDL_GetWindowFlags(Window) & SDL_WINDOW_FULLSCREEN) != 0;
}

void Rr_ToggleFullscreen(Rr_App *App)
{
    SDL_SetWindowFullscreen(App->Window, !Rr_IsAnyFullscreen(App->Window));
}

void Rr_SetInputConfig(Rr_App *App, Rr_InputConfig *InputConfig)
{
    App->InputConfig = *InputConfig;
}

Rr_InputState Rr_GetInputState(Rr_App *App)
{
    return App->InputState;
}

Rr_IntVec2 Rr_GetSwapchainSize(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    return (Rr_IntVec2){
        (int32_t)Renderer->SwapchainSize.width,
        (int32_t)Renderer->SwapchainSize.height,
    };
}

float Rr_GetAspectRatio(Rr_App *App)
{
    Rr_Renderer *Renderer = &App->Renderer;
    return (float)Renderer->SwapchainSize.width / (float)Renderer->SwapchainSize.height;
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

void *Rr_CreateObject(Rr_App *App)
{
    SDL_LockSpinlock(&App->ObjectStorage.Lock);

    Rr_Object *NewObject = (Rr_Object *)App->ObjectStorage.FreeObject;
    if(NewObject == NULL)
    {
        NewObject = RR_ALLOC(App->PermanentArena, sizeof(Rr_Object));
    }
    else
    {
        App->ObjectStorage.FreeObject = NewObject->Next;
    }
    App->ObjectStorage.ObjectCount++;

    SDL_UnlockSpinlock(&App->ObjectStorage.Lock);

    return RR_ZERO_PTR(NewObject);
}

void Rr_DestroyObject(Rr_App *App, void *Object)
{
    SDL_LockSpinlock(&App->ObjectStorage.Lock);

    Rr_Object *DestroyedObject = (Rr_Object *)Object;
    DestroyedObject->Next = App->ObjectStorage.FreeObject;
    App->ObjectStorage.FreeObject = DestroyedObject;
    App->ObjectStorage.ObjectCount--;

    SDL_UnlockSpinlock(&App->ObjectStorage.Lock);
}
