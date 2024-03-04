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

typedef struct SRrApp
{
    SDL_AtomicInt bExit;
    SDL_Window* Window;
    SDL_Thread* RenderThread;
    SDL_Semaphore* RenderThreadSemaphore;
    SRr Rr;
} SRrApp;

static void ShowDebugOverlay()
{
    static int location = 0;
    ImGuiIO* io = igGetIO();
    const ImGuiViewport* viewport = igGetMainViewport();
    ImGuiWindowFlags window_flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
    if (location >= 0)
    {
        const float PAD = 10.0f;
        ImVec2 work_pos = viewport->WorkPos; // Use work area to avoid menu-bar/task-bar, if any!
        ImVec2 work_size = viewport->WorkSize;
        ImVec2 window_pos, window_pos_pivot;
        window_pos.x = (location & 1) ? (work_pos.x + work_size.x - PAD) : (work_pos.x + PAD);
        window_pos.y = (location & 2) ? (work_pos.y + work_size.y - PAD) : (work_pos.y + PAD);
        window_pos_pivot.x = (location & 1) ? 1.0f : 0.0f;
        window_pos_pivot.y = (location & 2) ? 1.0f : 0.0f;
        igSetNextWindowPos(window_pos, ImGuiCond_Always, window_pos_pivot);
        igSetNextWindowViewport(viewport->ID);
        window_flags |= ImGuiWindowFlags_NoMove;
    }
    igSetNextWindowBgAlpha(0.35f); // Transparent background
    if (igBegin("Example: Simple overlay", NULL, window_flags))
    {
        igText("SDL Allocations: %zu", SDL_GetNumAllocations());
        igSeparator();
        if (igIsMousePosValid(NULL))
        {
            igText("Mouse Position: (%.1f,%.1f)", io->MousePos.x, io->MousePos.y);
        }
        else
        {
            igText("Mouse Position: <invalid>");
        }
    }
    igEnd();
}

static int RrApp_Render(void* AppPtr)
{
    SRrApp* App = (SRrApp*)AppPtr;

    SRrAsset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    SRrRawMesh RawMesh = RrRawMesh_FromOBJAsset(&DoorFrameOBJ);

    Rr_Init(&App->Rr, App->Window);
    Rr_SetMesh(&App->Rr, &RawMesh);
    Rr_InitImGui(&App->Rr, App->Window);

#ifdef SDL_PLATFORM_LINUX
    SDL_ShowWindow(App->Window);
#endif

    SDL_PostSemaphore(App->RenderThreadSemaphore);

    while (SDL_AtomicGet(&App->bExit) == false)
    {
        if (Rr_NewFrame(&App->Rr, App->Window))
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            igNewFrame();

            // igShowDemoWindow(NULL);
            ShowDebugOverlay();

            igRender();

            Rr_Draw(&App->Rr);
        }

        SDL_PostSemaphore(App->RenderThreadSemaphore);
    }

    RrRawMesh_Cleanup(&RawMesh);

    Rr_Cleanup(&App->Rr);

    return 0;
}

static int RrApp_Update(void* AppPtr)
{
    SRrApp* App = (SRrApp*)AppPtr;

    u64 Last = SDL_GetTicks();
    u64 Now = Last;
    u64 Frames = 0;
    f64 DeltaTime = 0;
    f64 Seconds = 0;
    f64 Acc = 0;
    while (SDL_AtomicGet(&App->bExit) == false)
    {
        Last = Now;
        Now = SDL_GetTicks();
        DeltaTime = (f64)(Now - Last) / 1000.0 * 1.0;
        Seconds += DeltaTime;
        Acc += DeltaTime;
        if (Acc >= 1.0)
        {
            Acc -= 1.0;
            SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "FPS: %llu", Frames);
            Frames = 0;
        }
        Frames++;

        for (SDL_Event Event; SDL_PollEvent(&Event);)
        {
            switch (Event.type)
            {
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    SDL_AtomicSet(&App->Rr.Swapchain.bShouldResize, true);
                }
                break;
                case SDL_EVENT_QUIT:
                {
                    SDL_AtomicSet(&App->bExit, true);
                }
                break;
            }

            ImGui_ImplSDL3_ProcessEvent(&Event);
        }

        SDL_WaitSemaphore(App->RenderThreadSemaphore);
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
        .RenderThread = SDL_CreateThread(RrApp_Render, "rt", &App),
        .RenderThreadSemaphore = SDL_CreateSemaphore(0),
    };

    SDL_WaitSemaphore(App.RenderThreadSemaphore);

    /* Showing the window from render thread works on Linux, not on Windows. */
#ifndef SDL_PLATFORM_LINUX
    SDL_ShowWindow(App.Window);
#endif

    RrApp_Update(&App);

    SDL_WaitThread(App.RenderThread, NULL);

    SDL_DestroySemaphore(App.RenderThreadSemaphore);

    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}
