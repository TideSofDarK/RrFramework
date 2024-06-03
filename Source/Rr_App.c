#include "Rr_App.h"

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
        igText("Reference Resolution: %dx%d", App->Renderer.ReferenceResolution.width, App->Renderer.ReferenceResolution.height);
        igText("Active Resolution: %dx%d", App->Renderer.ActiveResolution.width, App->Renderer.ActiveResolution.height);
        igText("SDL Allocations: %zu", SDL_GetNumAllocations());
        igText("RrFramework Objects: %zu", App->Renderer.ObjectCount);
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

    const SDL_DisplayID DisplayID = SDL_GetDisplayForWindow(Window);
    const SDL_DisplayMode* Mode = SDL_GetDesktopDisplayMode(DisplayID);
    FrameTime->TargetFramerate = (u64)Mode->refresh_rate;
    FrameTime->StartTime = SDL_GetTicksNS();
}

static int SDLCALL Rr_LoadingThreadProc(void* Data)
{
    Rr_App* App = Data;
    Rr_Renderer* Renderer = &App->Renderer;
    Rr_LoadingThread* LoadingThread = &App->LoadingThread;

    Rr_LoadCommandPools LoadCommandPools = { 0 };

    vkCreateCommandPool(Renderer->Device, &(VkCommandPoolCreateInfo){
                                              .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                              .pNext = VK_NULL_HANDLE,
                                              .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                              .queueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
                                          },
        NULL, &LoadCommandPools.GraphicsCommandPool);
    if (!Rr_IsUsingTransferQueue(Renderer))
    {
        vkCreateCommandPool(Renderer->Device, &(VkCommandPoolCreateInfo){
                                                  .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                                                  .pNext = VK_NULL_HANDLE,
                                                  .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
                                                  .queueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                                              },
            NULL, &LoadCommandPools.TransferCommandPool);
    }
    vkCreateFence(
        Renderer->Device,
        &(VkFenceCreateInfo){
            .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadCommandPools.Fence);
    vkCreateSemaphore(
        Renderer->Device,
        &(VkSemaphoreCreateInfo){
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
            .pNext = NULL,
            .flags = 0,
        },
        NULL,
        &LoadCommandPools.Semaphore);

    usize CurrentLoadingContextIndex = 0;

    while (SDL_AtomicGet(&App->bExit) == false)
    {
        if (SDL_WaitSemaphoreTimeout(LoadingThread->Semaphore, 500))
        {
            if (LoadingThread->LoadingContextsSlice.Length == 0)
            {
                continue;
            }

            Rr_LoadAsync_Internal(&LoadingThread->LoadingContextsSlice.Data[CurrentLoadingContextIndex], LoadCommandPools);
            CurrentLoadingContextIndex++;

            vkResetCommandPool(Renderer->Device, LoadCommandPools.GraphicsCommandPool, 0);
            if (LoadCommandPools.TransferCommandPool != VK_NULL_HANDLE)
            {
                vkResetCommandPool(Renderer->Device, LoadCommandPools.TransferCommandPool, 0);
            }
            vkResetFences(Renderer->Device, 1, &LoadCommandPools.Fence);

            SDL_LockMutex(LoadingThread->Mutex);
            if (CurrentLoadingContextIndex >= LoadingThread->LoadingContextsSlice.Length)
            {
                Rr_ResetArena(&LoadingThread->Arena);
                CurrentLoadingContextIndex = 0;
                Rr_SliceClear(&LoadingThread->LoadingContextsSlice);
            }
            SDL_UnlockMutex(LoadingThread->Mutex);
        }
    }

    vkDestroyCommandPool(Renderer->Device, LoadCommandPools.GraphicsCommandPool, NULL);
    if (LoadCommandPools.TransferCommandPool != VK_NULL_HANDLE)
    {
        vkDestroyCommandPool(Renderer->Device, LoadCommandPools.TransferCommandPool, NULL);
    }
    vkDestroyFence(Renderer->Device, LoadCommandPools.Fence, NULL);
    vkDestroySemaphore(Renderer->Device, LoadCommandPools.Semaphore, NULL);

    SDL_CleanupTLS();

    return EXIT_SUCCESS;
}

static void Rr_InitLoadingThread(Rr_App* App)
{
    App->LoadingThread = (Rr_LoadingThread){ .Semaphore = SDL_CreateSemaphore(0), .Mutex = SDL_CreateMutex(), .Arena = Rr_CreateArena(RR_LOADING_THREAD_ARENA_SIZE) };
    App->LoadingThread.Handle = SDL_CreateThread(Rr_LoadingThreadProc, "lt", App);
}

void Rr_Run(Rr_AppConfig* Config)
{
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(NULL);

    Rr_App* App = Rr_StackAlloc(Rr_App, 1);
    SDL_zerop(App);
    *App = (Rr_App){
        .Config = Config,
        .Window = SDL_CreateWindow(
            Config->Title,
            Config->ReferenceResolution.X,
            Config->ReferenceResolution.Y,
            SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN),
        .InputConfig = {
            .Count = Config->InputConfig->Count,
            .Mappings = Config->InputConfig->Mappings },
        .PermanentArena = Rr_CreateArena(RR_PERMANENT_ARENA_SIZE),
        .SyncArena = Rr_CreateSyncArena(RR_SYNC_ARENA_SIZE),
        .ScratchArenaTLS = SDL_CreateTLS()
    };

    InitFrameTime(&App->FrameTime, App->Window);

    SDL_SetEventEnabled(SDL_EVENT_DROP_FILE, true);

    SDL_AddEventWatch(EventWatch, App);

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
                break;
                default:
                    break;
            }
        }

        Iterate(App);
    }

    SDL_WaitThread(App->LoadingThread.Handle, NULL);
    SDL_DestroySemaphore(App->LoadingThread.Semaphore);
    SDL_DestroyMutex(App->LoadingThread.Mutex);

    Rr_CleanupRenderer(App);

    Rr_DestroyArena(&App->PermanentArena);
    Rr_DestroySyncArena(&App->SyncArena);
    SDL_CleanupTLS();

    SDL_DelEventWatch((SDL_EventFilter)EventWatch, App);
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
    return (float)Renderer->ActiveResolution.width / (float)Renderer->ActiveResolution.height;
}
