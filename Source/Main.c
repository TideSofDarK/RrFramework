#include <SDL3/SDL_main.h>

#include <Rr/RrApp.h>

int main(int argc, char** argv)
{
    Rr_AppConfig Config = {
        .Title = "VulkanPlayground"
    };
    Rr_Run(&Config);

    return 0;
}
