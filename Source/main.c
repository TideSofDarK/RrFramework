#include <SDL3/SDL_main.h>

#include "app.h"

int main(int argc, char** argv)
{
    SApp App;

    CreateApp(&App);
    RunApp(&App);

    return 0;
}
