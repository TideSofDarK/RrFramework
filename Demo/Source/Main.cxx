#include "Game.hxx"

#ifdef _MSC_VER
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
#else
int main()
#endif
{
    RunGame();
    return 0;
}