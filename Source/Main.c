#include <SDL3/SDL_main.h>

#include <Rr/RrApp.h>

int main(int argc, char** argv)
{
    SRrAppConfig Config = {
        .Title = "VulkanPlayground"
    };
    RrApp_Run(&Config);

    return 0;
}
