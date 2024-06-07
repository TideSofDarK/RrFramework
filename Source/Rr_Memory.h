#pragma once

#include <SDL3/SDL_mutex.h>

#include "Rr_Defines.h"

#ifdef __cplusplus
extern "C"
{
#endif

/*
 * Common Memory Functions
 */

#ifndef RR_DISABLE_ALLOCA
    #define Rr_StackAlloc(Type, Count) (Type*)alloca(sizeof(Type) * (Count))
    #define Rr_StackFree(Data) \
        do                     \
        {                      \
        }                      \
        while (0)
#else
    #define Rr_StackAlloc(Type, Count) (Type*)Rr_Calloc((Count), sizeof(Type))
    #define Rr_StackFree(Data) \
        do                     \
        {                      \
            Rr_Free(Data)      \
        }                      \
        while (0)
#endif

extern void* Rr_Malloc(usize Bytes);
extern void* Rr_Calloc(usize Num, usize Bytes);
extern void* Rr_Realloc(void* Ptr, usize Bytes);
extern void Rr_Free(void* Ptr);

extern void* Rr_AlignedAlloc(usize Alignment, usize Bytes);
extern void Rr_AlignedFree(void* Ptr);

/*
 * Arena
 */

#define RR_SCRATCH_ARENA_COUNT_PER_THREAD 2

typedef struct Rr_Arena Rr_Arena;
struct Rr_Arena
{
    byte* Allocation;
    byte* Current;
    byte* End;
};

extern Rr_Arena Rr_CreateArena(usize Size);
extern void Rr_ResetArena(Rr_Arena* Arena);
extern void Rr_DestroyArena(Rr_Arena* Arena);

extern void* Rr_ArenaAlloc(Rr_Arena* Arena, usize Size, usize Align, usize Count);

#define Rr_ArenaAllocOne(Arena, Size) Rr_ArenaAlloc(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define Rr_ArenaAllocCount(Arena, Size, Count) Rr_ArenaAlloc(Arena, Size, RR_SAFE_ALIGNMENT, Count)

typedef struct Rr_ArenaScratch Rr_ArenaScratch;
struct Rr_ArenaScratch
{
    Rr_Arena* Arena;
    byte* Position;
};

extern Rr_ArenaScratch Rr_CreateArenaScratch(Rr_Arena* Arena);
extern void Rr_DestroyArenaScratch(Rr_ArenaScratch Scratch);

extern void Rr_SetScratchTLS(void* TLSID);
extern void Rr_InitThreadScratch(usize Size);
extern Rr_ArenaScratch Rr_GetArenaScratch(Rr_Arena* Conflict);

/*
 * Sync Arena
 */

typedef struct Rr_SyncArena Rr_SyncArena;
struct Rr_SyncArena
{
    SDL_Mutex* Mutex;
    Rr_Arena Arena;
};

extern Rr_SyncArena Rr_CreateSyncArena(usize Size);
extern void Rr_DestroySyncArena(Rr_SyncArena* Arena);

/*
 * Dynamic Slice
 */

extern void Rr_SliceGrow(void* Slice, usize Size, Rr_Arena* Arena);

extern void Rr_SliceResize(void* Slice, usize Size, usize Count, Rr_Arena* Arena);

#define Rr_SliceType(Type) \
    struct                 \
    {                      \
        Type* Data;        \
        usize Length;      \
        usize Capacity;    \
    }

#define Rr_SlicePush(Slice, Arena)                                             \
    ((Slice)->Length >= (Slice)->Capacity                                      \
        ? Rr_SliceGrow((Slice), sizeof(*(Slice)->Data), (Arena)), /* NOLINT */ \
        (Slice)->Data + (Slice)->Length++                                      \
        : (Slice)->Data + (Slice)->Length++)

#define Rr_SlicePop(Slice) (Slice)->Length > 0 ? (Slice)->Length-- : (void)0

#define Rr_SliceReserve(Slice, ElementCount, Arena)                                    \
    ((Slice)->Capacity < (ElementCount)                                                \
            ? Rr_SliceResize((Slice), sizeof(*(Slice)->Data), (ElementCount), (Arena)) \
            : (void)0)

#define Rr_SliceClear(Slice) (Slice)->Length = 0, (Slice)->Capacity = 0, (Slice)->Data = NULL

#define Rr_SliceEmpty(Slice) (Slice)->Length = 0

#define Rr_SliceLength(Slice) (Slice)->Length

#define Rr_SliceDuplicate(Dst, Src, Arena)        \
    Rr_SliceReserve((Dst), (Src)->Length, Arena), \
        (Dst)->Length = (Src)->Length, SDL_memcpy((Dst)->Data, (Src)->Data, sizeof(*(Dst)->Data) * (Src)->Length)

#ifdef __cplusplus
}
#endif
