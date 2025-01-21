#pragma once

#include <Rr/Rr_Memory.h>

#include <SDL3/SDL_atomic.h>

/*
 * Sync Arena
 */

typedef struct Rr_SyncArena Rr_SyncArena;
struct Rr_SyncArena
{
    SDL_SpinLock Lock;
    Rr_Arena *Arena;
};

extern Rr_SyncArena Rr_CreateSyncArena(void);

extern void Rr_DestroySyncArena(Rr_SyncArena *Arena);
