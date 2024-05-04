#include <SDL3/SDL_main.h>

#include "Game.hxx"

static void RrDemo()
{
    RunGame();
}

#ifdef __cplusplus
extern "C"
#endif
    int
    main(int argc, char* argv[])
{
    RrDemo();

    return 0;
}