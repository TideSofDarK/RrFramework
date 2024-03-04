#include "RrApp.h"

#include "Rr/Rr.h"
#include "Rr/RrAsset.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_thread.h>
#include <SDL3/SDL_mutex.h>
#include <SDL3/SDL_atomic.h>

typedef struct SRrApp
{
    SDL_Window* Window;
    SDL_AtomicInt bExit;
    SDL_Semaphore* RenderLoopSemaphore;
    SDL_Semaphore* RenderInitSemaphore;
    SRr Rr;
} SRrApp;

static int RrApp_Render(void* AppPtr)
{
    SRrApp* App = (SRrApp*)AppPtr;

    SRrAsset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    SRrRawMesh RawMesh = RrRawMesh_FromOBJAsset(&DoorFrameOBJ);

    Rr_Init(&App->Rr, App->Window);
    Rr_SetMesh(&App->Rr, &RawMesh);
    Rr_InitImGui(&App->Rr, App->Window);

    SDL_ShowWindow(App->Window);

    SDL_PostSemaphore(App->RenderInitSemaphore);

    while (SDL_AtomicGet(&App->bExit) == false)
    {
        if (Rr_NewFrame(&App->Rr, App->Window))
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            igNewFrame();

            igShowDemoWindow(NULL);

            igRender();

            Rr_Draw(&App->Rr);
        }

        SDL_PostSemaphore(App->RenderLoopSemaphore);
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
            SDL_LogWarn(SDL_LOG_CATEGORY_SYSTEM, "FPS: %zu", Frames);
            Frames = 0;
        }
        Frames++;
        SDL_WaitSemaphore(App->RenderLoopSemaphore);

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
    }

    return 0;
}

void RrApp_Run(SRrAppConfig* Config)
{
    SRrApp App = { 0 };

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#ifdef RR_DEBUG
    RrArray_Test();
#endif

    App.RenderLoopSemaphore = SDL_CreateSemaphore(0);
    App.RenderInitSemaphore = SDL_CreateSemaphore(0);

    SDL_Vulkan_LoadLibrary(NULL);

    App.Window = SDL_CreateWindow(
        Config->Title,
        1600,
        800,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

    SDL_Thread* RenderThread = SDL_CreateThread(RrApp_Render, "rt", &App);
    if (RenderThread == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "Could not create render thread! %s\n", SDL_GetError());
        abort();
    }

    SDL_WaitSemaphore(App.RenderInitSemaphore);

    RrApp_Update(&App);

    SDL_WaitThread(RenderThread, NULL);

    SDL_DestroySemaphore(App.RenderLoopSemaphore);
    SDL_DestroySemaphore(App.RenderInitSemaphore);

    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}
