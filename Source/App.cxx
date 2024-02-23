#include "App.hxx"
#include "Rr.hxx"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL_events.h>
#include <SDL_video.h>

extern "C" void RunApp()
{
    SApp App = {};

    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(nullptr);

    App.Window = SDL_CreateWindow(
        AppTitle,
        1600,
        800,
        SDL_WINDOW_VULKAN | SDL_WINDOW_RESIZABLE);

    RR_Init(&App.Rr, App.Window);

    while (!App.bExit)
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            switch (event.type)
            {
                case SDL_EVENT_WINDOW_RESIZED:
                    i32 Width, Height;
                    SDL_GetWindowSizeInPixels(App.Window, &Width, &Height);
                    RR_Resize(&App.Rr, Width, Height);
                    break;
                case SDL_EVENT_QUIT:
                    App.bExit = true;
                    break;
            }
        }

        RR_Draw(&App.Rr);
    }

    RR_Cleanup(&App.Rr);
    SDL_DestroyWindow(App.Window);
    SDL_Quit();
}
