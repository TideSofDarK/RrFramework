#include "Rr_Memory.h"
#include "Rr_Log.h"

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>

void* Rr_Malloc(Rr_USize Bytes)
{
    return SDL_malloc(Bytes);
}

void* Rr_Calloc(Rr_USize Num, Rr_USize Bytes)
{
    return SDL_calloc(Num, Bytes);
}

void* Rr_Realloc(void* Ptr, Rr_USize Bytes)
{
    return SDL_realloc(Ptr, Bytes);
}

void Rr_Free(void* Ptr)
{
    SDL_free(Ptr);
}

void* Rr_AlignedAlloc(Rr_USize Alignment, Rr_USize Bytes)
{
    return SDL_aligned_alloc(Alignment, Bytes);
}

void Rr_AlignedFree(void* Ptr)
{
    SDL_aligned_free(Ptr);
}

Rr_Arena Rr_CreateArena(Rr_USize Size)
{
    Rr_Byte* Allocation = Rr_Malloc(Size);
    return (Rr_Arena){
        .Allocation = Allocation,
        .Current = Allocation,
        .End = Allocation + Size
    };
}

void Rr_ResetArena(Rr_Arena* Arena)
{
    Arena->Current = Arena->Allocation;
}

void Rr_DestroyArena(Rr_Arena* Arena)
{
    if (Arena == NULL)
    {
        return;
    }
    Rr_Free(Arena->Allocation);
}

Rr_ArenaScratch Rr_CreateArenaScratch(Rr_Arena* Arena)
{
    return (Rr_ArenaScratch){
        .Arena = Arena,
        .Position = Arena->Current
    };
}

void Rr_DestroyArenaScratch(Rr_ArenaScratch Scratch)
{
    Scratch.Arena->Current = Scratch.Position;
}

static void SDLCALL Rr_CleanupScratchArena(void* ScratchArena)
{
    Rr_Arena* Arenas = ScratchArena;
    for (Rr_USize Index = 0; Index < RR_SCRATCH_ARENA_COUNT_PER_THREAD; ++Index)
    {
        Rr_DestroyArena(Arenas + Index);
    }
    Rr_Free(ScratchArena);
}

static SDL_TLSID ScratchArenaTLS;

void Rr_SetScratchTLS(void* TLSID)
{
    ScratchArenaTLS = *((SDL_TLSID*)TLSID);
}

void Rr_InitThreadScratch(Rr_USize Size)
{
    if (ScratchArenaTLS == 0)
    {
        Rr_LogAbort("ScratchArenaTLS is not set!");
    }
    Rr_Arena* Arena = Rr_Calloc(RR_SCRATCH_ARENA_COUNT_PER_THREAD, sizeof(Rr_Arena));
    for (Rr_USize Index = 0; Index < RR_SCRATCH_ARENA_COUNT_PER_THREAD; ++Index)
    {
        Arena[Index] = Rr_CreateArena(Size);
    }
    SDL_SetTLS(ScratchArenaTLS, Arena, Rr_CleanupScratchArena);
}

Rr_ArenaScratch Rr_GetArenaScratch(Rr_Arena* Conflict)
{
    if (ScratchArenaTLS == 0)
    {
        Rr_LogAbort("ScratchArenaTLS is not set!");
    }
    Rr_Arena* Arena = (Rr_Arena*)SDL_GetTLS(ScratchArenaTLS);
    if (Conflict == NULL)
    {
        return Rr_CreateArenaScratch(Arena);
    }
    else
    {
        for (Rr_USize Index = 0; Index < RR_SCRATCH_ARENA_COUNT_PER_THREAD; ++Index)
        {
            if (Arena + Index != Conflict)
            {
                return Rr_CreateArenaScratch(Arena + Index);
            }
        }
    }

    Rr_LogAbort("Couldn't find appropriate arena for a scratch!");

    return (Rr_ArenaScratch){ 0 };
}

void* Rr_ArenaAlloc(Rr_Arena* Arena, Rr_USize Size, Rr_USize Align, Rr_USize Count)
{
    Rr_USize Padding = -(Rr_USize)Arena->Current & (Align - 1);
    Rr_ISize Available = (Arena->End - Arena->Current) - (Rr_ISize)Padding;
    if (Available < 0 || Count > Available / Size)
    {
        Rr_LogAbort("Running out of arena memory!");
    }
    Rr_Byte* p = Arena->Current + Padding;
    Arena->Current += Padding + Count * Size;
    return memset(p, 0, Count * Size);
}

Rr_SyncArena Rr_CreateSyncArena(Rr_USize Size)
{
    return (Rr_SyncArena){
        .Arena = Rr_CreateArena(Size)
    };
}

void Rr_DestroySyncArena(Rr_SyncArena* Arena)
{
    Rr_DestroyArena(&Arena->Arena);
}

void Rr_SliceGrow(void* Slice, Rr_USize Size, Rr_Arena* Arena)
{
    if (Arena == NULL)
    {
        Rr_LogAbort("Attempt to grow a slice but Arena is NULL!");
    }

    Rr_SliceType(void) Replica;
    memcpy(&Replica, Slice, sizeof(Replica));

    void* Data = NULL;
    Replica.Capacity = Replica.Capacity ? Replica.Capacity : 1;
    Replica.Capacity *= 2;
    Data = Rr_ArenaAllocCount(Arena, Size, Replica.Capacity);

    if (Replica.Length)
    {
        memcpy(Data, Replica.Data, Size * Replica.Length);
    }
    Replica.Data = Data;

    memcpy(Slice, &Replica, sizeof(Replica));
}

void Rr_SliceResize(void* Slice, Rr_USize Size, Rr_USize Count, Rr_Arena* Arena)
{
    if (Arena == NULL)
    {
        Rr_LogAbort("Attempt to grow a slice but Arena is NULL!");
    }

    Rr_SliceType(void) Replica;
    memcpy(&Replica, Slice, sizeof(Replica));

    void* Data = NULL;

    Replica.Capacity = Count;
    Data = Rr_ArenaAllocCount(Arena, Size, Count);

    if (Replica.Length)
    {
        memcpy(Data, Replica.Data, Size * Replica.Length);
    }
    Replica.Data = Data;

    memcpy(Slice, &Replica, sizeof(Replica));
}
