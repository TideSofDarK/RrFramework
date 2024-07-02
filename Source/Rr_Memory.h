#pragma once

#include <SDL3/SDL_thread.h>

#include "Rr/Rr_Defines.h"

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Memory Functions
 */

#ifndef RR_DISABLE_ALLOCA
#define Rr_StackAlloc(Type, Count) (Type *)alloca(sizeof(Type) * (Count))
#define Rr_StackFree(Data)                                                     \
    do                                                                         \
    {                                                                          \
    }                                                                          \
    while (0)
#else
#define Rr_StackAlloc(Type, Count) (Type *)Rr_Calloc((Count), sizeof(Type))
#define Rr_StackFree(Data)                                                     \
    do                                                                         \
    {                                                                          \
        Rr_Free(Data)                                                          \
    }                                                                          \
    while (0)
#endif

extern void *Rr_Malloc(uintptr_t Bytes);

extern void *Rr_Calloc(uintptr_t Num, uintptr_t Bytes);

extern void *Rr_Realloc(void *Ptr, uintptr_t Bytes);

extern void Rr_Free(void *Ptr);

extern void *Rr_AlignedAlloc(uintptr_t Alignment, uintptr_t Bytes);

extern void Rr_AlignedFree(void *Ptr);

/*
 * Arena
 */

#define RR_SCRATCH_ARENA_COUNT_PER_THREAD 2

typedef struct Rr_Arena Rr_Arena;
struct Rr_Arena
{
    char *Allocation;
    char *Current;
    char *End;
};

extern Rr_Arena Rr_CreateArena(uintptr_t Size);

extern void Rr_ResetArena(Rr_Arena *Arena);

extern void Rr_DestroyArena(Rr_Arena *Arena);

extern void *Rr_ArenaAlloc(
    Rr_Arena *Arena,
    uintptr_t Size,
    uintptr_t Align,
    uintptr_t Count);

#define Rr_ArenaAllocOne(Arena, Size)                                          \
    Rr_ArenaAlloc(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define Rr_ArenaAllocCount(Arena, Size, Count)                                 \
    Rr_ArenaAlloc(Arena, Size, RR_SAFE_ALIGNMENT, Count)

typedef struct Rr_ArenaScratch Rr_ArenaScratch;
struct Rr_ArenaScratch
{
    Rr_Arena *Arena;
    char *Position;
};

extern Rr_ArenaScratch Rr_CreateArenaScratch(Rr_Arena *Arena);

extern void Rr_DestroyArenaScratch(Rr_ArenaScratch Scratch);

extern void Rr_SetScratchTLS(void *TLSID);

extern void Rr_InitThreadScratch(uintptr_t Size);

extern Rr_ArenaScratch Rr_GetArenaScratch(Rr_Arena *Conflict);

/*
 * Sync Arena
 */

typedef struct Rr_SyncArena Rr_SyncArena;
struct Rr_SyncArena
{
    SDL_SpinLock Lock;
    Rr_Arena Arena;
};

extern Rr_SyncArena Rr_CreateSyncArena(uintptr_t Size);

extern void Rr_DestroySyncArena(Rr_SyncArena *Arena);

/*
 * Dynamic Slice
 */

extern void Rr_SliceGrow(void *Slice, uintptr_t Size, Rr_Arena *Arena);

extern void Rr_SliceResize(
    void *Slice,
    uintptr_t Size,
    uintptr_t Count,
    Rr_Arena *Arena);

#define Rr_SliceType(Type)                                                     \
    struct                                                                     \
    {                                                                          \
        Type *Data;                                                            \
        uintptr_t Length;                                                       \
        uintptr_t Capacity;                                                     \
    }

#define Rr_SlicePush(Slice, Arena)                                             \
    ((Slice)->Length >= (Slice)->Capacity                                      \
     ? Rr_SliceGrow((Slice), sizeof(*(Slice)->Data), (Arena)), /* NOLINT */    \
     (Slice)->Data + (Slice)->Length++                                         \
     : (Slice)->Data + (Slice)->Length++)

#define Rr_SlicePop(Slice) (Slice)->Length > 0 ? (Slice)->Length-- : (void)0

#define Rr_SliceReserve(Slice, ElementCount, Arena)                            \
    ((Slice)->Capacity < (ElementCount) ? Rr_SliceResize(                      \
                                              (Slice),                         \
                                              sizeof(*(Slice)->Data),          \
                                              (ElementCount),                  \
                                              (Arena))                         \
                                        : (void)0)

#define Rr_SliceClear(Slice)                                                   \
    (Slice)->Length = 0, (Slice)->Capacity = 0, (Slice)->Data = NULL

#define Rr_SliceEmpty(Slice) (Slice)->Length = 0

#define Rr_SliceLength(Slice) (Slice)->Length

#define Rr_SliceDuplicate(Dst, Src, Arena)                                     \
    Rr_SliceReserve((Dst), (Src)->Length, Arena),                              \
        (Dst)->Length = (Src)->Length,                                         \
        SDL_memcpy(                                                            \
            (Dst)->Data,                                                       \
            (Src)->Data,                                                       \
            sizeof(*(Dst)->Data) * (Src)->Length)

#ifdef __cplusplus
}
#endif
