#include "RrMemory.h"

#include <SDL3/SDL_stdinc.h>

void* Rr_Malloc(size_t Bytes)
{
    return SDL_malloc(Bytes);
}

void* Rr_Calloc(size_t Num, size_t Bytes)
{
    return SDL_calloc(Num, Bytes);
}

void* Rr_Realloc(void* Ptr, size_t Bytes)
{
    return SDL_realloc(Ptr, Bytes);
}

void Rr_Free(void* Ptr)
{
    SDL_free(Ptr);
}
