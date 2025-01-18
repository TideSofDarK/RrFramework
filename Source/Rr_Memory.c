#include "Rr_Memory.h"

#include "Rr_Log.h"

#include <Rr/Rr_Platform.h>

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>

#include <assert.h>

void *Rr_Malloc(size_t Bytes)
{
    return SDL_malloc(Bytes);
}

void *Rr_Calloc(size_t Num, size_t Bytes)
{
    return SDL_calloc(Num, Bytes);
}

void *Rr_Realloc(void *Ptr, size_t Bytes)
{
    return SDL_realloc(Ptr, Bytes);
}

void Rr_Free(void *Ptr)
{
    SDL_free(Ptr);
}

void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes)
{
    return SDL_aligned_alloc(Alignment, Bytes);
}

void Rr_AlignedFree(void *Ptr)
{
    SDL_aligned_free(Ptr);
}

Rr_Arena *Rr_CreateArena(size_t ReserveSize, size_t CommitSize)
{
    size_t PageSize = Rr_GetPlatformInfo()->PageSize;
    ReserveSize = RR_ALIGN_POW2(ReserveSize, PageSize);
    CommitSize = RR_ALIGN_POW2(CommitSize, PageSize);

    char *Data = Rr_ReserveMemory(ReserveSize);
    bool Success = Rr_CommitMemory(Data, CommitSize);
    assert(Success);

    Rr_Arena *Arena = (Rr_Arena *)Data;
    *Arena = (Rr_Arena){
        .Position = sizeof(Rr_Arena),
        .ReserveSize = ReserveSize,
        .CommitSize = CommitSize,
        .Reserved = ReserveSize,
        .Commited = CommitSize,
    };

    return Arena;
}

Rr_Arena *Rr_CreateArenaDefault()
{
    return Rr_CreateArena(RR_ARENA_RESERVE_DEFAULT, RR_ARENA_COMMIT_DEFAULT);
}

void Rr_ResetArena(Rr_Arena *Arena)
{
    Arena->Position = sizeof(Rr_Arena);
}

void Rr_DestroyArena(Rr_Arena *Arena)
{
    if(Arena == NULL)
    {
        return;
    }
    Rr_ReleaseMemory((void *)Arena, Arena->ReserveSize);
}

Rr_ArenaScratch Rr_CreateArenaScratch(Rr_Arena *Arena)
{
    return (Rr_ArenaScratch){ .Arena = Arena, .Position = Arena->Position };
}

void Rr_DestroyArenaScratch(Rr_ArenaScratch Scratch)
{
    Scratch.Arena->Position = Scratch.Position;
}

static void SDLCALL Rr_CleanupScratchArena(void *ScratchArena)
{
    Rr_Arena **Arenas = ScratchArena;
    for(size_t Index = 0; Index < 2; ++Index)
    {
        Rr_DestroyArena(Arenas[Index]);
    }
    Rr_Free(ScratchArena);
}

static SDL_TLSID ScratchArenaTLS;

void Rr_SetScratchTLS(void *TLSID)
{
    ScratchArenaTLS = *((SDL_TLSID *)TLSID);
}

void Rr_InitThreadScratch(size_t Size)
{
    if(SDL_GetTLS(&ScratchArenaTLS) != 0)
    {
        Rr_LogAbort("Scratch is already initialized for this thread!");
    }
    Rr_Arena **Arenas = Rr_Calloc(2, sizeof(Rr_Arena *));
    for(size_t Index = 0; Index < 2; ++Index)
    {
        Arenas[Index] = Rr_CreateArenaDefault();
    }
    SDL_SetTLS(&ScratchArenaTLS, Arenas, Rr_CleanupScratchArena);
}

Rr_ArenaScratch Rr_GetArenaScratch(Rr_Arena *Conflict)
{
    if(ScratchArenaTLS.value == 0)
    {
        Rr_LogAbort("ScratchArenaTLS is not set!");
    }
    Rr_Arena **Arenas = (Rr_Arena **)SDL_GetTLS(&ScratchArenaTLS);
    if(Conflict == NULL)
    {
        return Rr_CreateArenaScratch(Arenas[0]);
    }
    else
    {
        for(size_t Index = 0; Index < 2; ++Index)
        {
            if(Arenas[Index] != Conflict)
            {
                return Rr_CreateArenaScratch(Arenas[Index]);
            }
        }
    }

    Rr_LogAbort("Couldn't find appropriate arena for a scratch!");

    return (Rr_ArenaScratch){ 0 };
}

void *Rr_ArenaAlloc(Rr_Arena *Arena, size_t Size, size_t Align, size_t Count)
{
    size_t TotalSize = Size * Count;
    uintptr_t PositionAligned = RR_ALIGN_POW2(Arena->Position, Align);
    uintptr_t Target = PositionAligned + TotalSize;

    if(Arena->Commited < Target)
    {
        uintptr_t CommitTarget = Target + Arena->CommitSize - 1;
        CommitTarget -= CommitTarget % Arena->CommitSize;
        CommitTarget = RR_MIN(CommitTarget, Arena->Reserved);
        uintptr_t CommitSize = CommitTarget - Arena->Commited;
        char *CommitPtr = (char *)Arena + Arena->Commited;
        Rr_CommitMemory(CommitPtr, CommitSize);
        Arena->Commited = CommitTarget;
    }

    char *Result = NULL;
    if(Arena->Commited >= Target)
    {
        Result = (char *)Arena + PositionAligned;
        Arena->Position = Target;
    }
    else
    {
        Rr_LogAbort("Arena reserved memory overflow!");
    }

    return memset(Result, 0, TotalSize);
}

Rr_SyncArena Rr_CreateSyncArena()
{
    return (Rr_SyncArena){ .Arena = Rr_CreateArenaDefault() };
}

void Rr_DestroySyncArena(Rr_SyncArena *Arena)
{
    Rr_DestroyArena(Arena->Arena);
}

void Rr_SliceGrow(void *Slice, size_t Size, Rr_Arena *Arena)
{
    if(Arena == NULL)
    {
        Rr_LogAbort("Attempt to grow a slice but Arena is NULL!");
    }

    RR_SLICE_TYPE(void) Replica;
    memcpy(&Replica, Slice, sizeof(Replica));

    void *Data = NULL;
    Replica.Capacity = Replica.Capacity ? Replica.Capacity : 1;
    Replica.Capacity *= 2;
    Data = RR_ARENA_ALLOC_COUNT(Arena, Size, Replica.Capacity);

    if(Replica.Count)
    {
        memcpy(Data, Replica.Data, Size * Replica.Count);
    }
    Replica.Data = Data;

    memcpy(Slice, &Replica, sizeof(Replica));
}

void Rr_SliceResize(void *Slice, size_t Size, size_t Count, Rr_Arena *Arena)
{
    if(Arena == NULL)
    {
        Rr_LogAbort("Attempt to grow a slice but Arena is NULL!");
    }

    RR_SLICE_TYPE(void) Replica;
    memcpy(&Replica, Slice, sizeof(Replica));

    void *Data = NULL;

    Replica.Capacity = Count;
    Data = RR_ARENA_ALLOC_COUNT(Arena, Size, Count);

    if(Replica.Count)
    {
        memcpy(Data, Replica.Data, Size * Replica.Count);
    }
    Replica.Data = Data;

    memcpy(Slice, &Replica, sizeof(Replica));
}
