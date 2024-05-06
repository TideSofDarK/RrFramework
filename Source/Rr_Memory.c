#include "Rr_Memory.h"

#include <SDL3/SDL_stdinc.h>

void* Rr_Malloc(const size_t Bytes)
{
    return SDL_malloc(Bytes);
}

void* Rr_Calloc(const size_t Num, const size_t Bytes)
{
    return SDL_calloc(Num, Bytes);
}

void* Rr_Realloc(void* Ptr, const size_t Bytes)
{
    return SDL_realloc(Ptr, Bytes);
}

void Rr_Free(void* Ptr)
{
    SDL_free(Ptr);
}

void* Rr_AlignedAlloc(size_t Alignment, size_t Bytes)
{
    return SDL_aligned_alloc(Alignment, Bytes);
}

void Rr_AlignedFree(void* Ptr)
{
    SDL_aligned_free(Ptr);
}
