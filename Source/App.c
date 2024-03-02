#include "App.h"
#include "Rr/Rr.h"

#define CIMGUI_DEFINE_ENUMS_AND_STRUCTS
#include <imgui/cimgui.h>
#include <imgui/cimgui_impl.h>

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL_events.h>
#include <SDL_video.h>

#include "Rr/RrAsset.h"

const char* AppTitle = "VulkanPlayground";

void RunApp(void)
{
    SApp App = { 0 };

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

#ifdef RR_DEBUG
    RrArray_Test();
#endif

    SRrAsset DoorFrameOBJ;
    RrAsset_Extern(&DoorFrameOBJ, DoorFrameOBJ);

    SRrRawMesh RawMesh = RrRawMesh_FromOBJAsset(&DoorFrameOBJ);

    SDL_Vulkan_LoadLibrary(NULL);

    App.Window = SDL_CreateWindow(
        AppTitle,
        1600,
        800,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);

    Rr_Init(&App.Rr, App.Window);
    Rr_SetMesh(&App.Rr, &RawMesh);
    Rr_InitImGui(&App.Rr, App.Window);

    SDL_ShowWindow(App.Window);

    while (!App.bExit)
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            switch (event.type)
            {
                case SDL_EVENT_WINDOW_RESIZED:
                {
                    App.Rr.Swapchain.bShouldResize = true;
                }
                break;
                case SDL_EVENT_QUIT:
                {
                    App.bExit = true;
                }
                break;
            }

            ImGui_ImplSDL3_ProcessEvent(&event);
        }

        if (Rr_NewFrame(&App.Rr, App.Window))
        {
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            igNewFrame();

            // igShowDemoWindow(NULL);

            igRender();

            Rr_Draw(&App.Rr);
        }
    }

    Rr_Cleanup(&App.Rr);
    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}
