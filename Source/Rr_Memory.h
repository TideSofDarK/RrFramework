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
#define Rr_StackFree(Data) \
    do                     \
    {                      \
    }                      \
    while (0)
#else
#define Rr_StackAlloc(Type, Count) (Type *)Rr_Calloc((Count), sizeof(Type))
#define Rr_StackFree(Data) \
    do                     \
    {                      \
        Rr_Free(Data)      \
    }                      \
    while (0)
#endif

extern void *Rr_Malloc(size_t Bytes);

extern void *Rr_Calloc(size_t Num, size_t Bytes);

extern void *Rr_Realloc(void *Ptr, size_t Bytes);

extern void Rr_Free(void *Ptr);

extern void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes);

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

extern Rr_Arena Rr_CreateArena(size_t Size);

extern void Rr_ResetArena(Rr_Arena *Arena);

extern void Rr_DestroyArena(Rr_Arena *Arena);

extern void *Rr_ArenaAlloc(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count);

#define RR_ARENA_ALLOC_ONE(Arena, Size) \
    Rr_ArenaAlloc(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ARENA_ALLOC_COUNT(Arena, Size, Count) \
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

extern void Rr_InitThreadScratch(size_t Size);

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

extern Rr_SyncArena Rr_CreateSyncArena(size_t Size);

extern void Rr_DestroySyncArena(Rr_SyncArena *Arena);

/*
 * Dynamic Slice
 */

extern void Rr_SliceGrow(void *Slice, size_t Size, Rr_Arena *Arena);

extern void Rr_SliceResize(
    void *Slice,
    size_t Size,
    size_t Count,
    Rr_Arena *Arena);

#define RR_SLICE_TYPE(Type) \
    struct                  \
    {                       \
        Type *Data;         \
        size_t Length;      \
        size_t Capacity;    \
    }

#define RR_SLICE_PUSH(Slice, Arena)                                         \
    ((Slice)->Length >= (Slice)->Capacity                                   \
     ? Rr_SliceGrow((Slice), sizeof(*(Slice)->Data), (Arena)), /* NOLINT */ \
     (Slice)->Data + (Slice)->Length++                                      \
     : (Slice)->Data + (Slice)->Length++)

#define RR_SLICE_POP(Slice) (Slice)->Length > 0 ? (Slice)->Length-- : (void)0

#define RR_SLICE_RESERVE(Slice, ElementCount, Arena) \
    ((Slice)->Capacity < (ElementCount)              \
         ? Rr_SliceResize(                           \
               (Slice),                              \
               sizeof(*(Slice)->Data), /* NOLINT */  \
               (ElementCount),                       \
               (Arena))                              \
         : (void)0)

#define RR_SLICE_EMPTY(Slice) (Slice)->Length = 0

#define RR_SLICE_LENGTH(Slice) (Slice)->Length

#define RR_SLICE_DUPLICATE(Dst, Src, Arena)       \
    Rr_SliceReserve((Dst), (Src)->Length, Arena), \
        (Dst)->Length = (Src)->Length,            \
        SDL_memcpy(                               \
            (Dst)->Data,                          \
            (Src)->Data,                          \
            sizeof(*(Dst)->Data) * (Src)->Length)

/*
 * Hashmap
 */

typedef struct Rr_Map Rr_Map;
struct Rr_Map
{
    Rr_Map *Child[4];
    uintptr_t Key;
    void *Value;
};

inline void **Rr_MapUpsert(Rr_Map **Map, uintptr_t Key, Rr_Arena *Arena)
{
    if (*Map != NULL)
    {
        for (uintptr_t Hash = Key; *Map; Hash <<= 2)
        {
            if (Key == (*Map)->Key)
            {
                return &(*Map)->Value;
            }
            Map = &(*Map)->Child[Hash >> 62];
        }
    }
    if (Arena == NULL)
    {
        return NULL;
    }
    *Map = (Rr_Map *)RR_ARENA_ALLOC_ONE(Arena, sizeof(Rr_Map));
    (*Map)->Key = Key;
    return &(*Map)->Value;
}

#ifdef __cplusplus
}
#endif
