#include "app.h"
#include "renderer.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_vulkan.h>
#include <SDL_video.h>

void CreateApp(SApp* App)
{
    SDL_LogSetAllPriority(SDL_LOG_PRIORITY_INFO);
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);

    SDL_Vulkan_LoadLibrary(nullptr);

    App->Window = SDL_CreateWindow(
        AppTitle,
        1600,
        800,
        SDL_WINDOW_VULKAN);

    RendererInit(App);
}

void RunApp(SApp* App)
{
    while (!App->bExit)
    {
        for (SDL_Event event; SDL_PollEvent(&event);)
        {
            switch (event.type)
            {
                case SDL_EVENT_QUIT:
                    App->bExit = true;
                    break;
            }
        }
    }

    RendererCleanup(App);
    SDL_DestroyWindow(App->Window);
}
