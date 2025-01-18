#pragma once

#include <Rr/Rr_Defines.h>

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
    while(0)
#else
#define Rr_StackAlloc(Type, Count) (Type *)Rr_Calloc((Count), sizeof(Type))
#define Rr_StackFree(Data) \
    do                     \
    {                      \
        Rr_Free(Data)      \
    }                      \
    while(0)
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

#define RR_ARENA_RESERVE_DEFAULT RR_GIGABYTES(8)
#define RR_ARENA_COMMIT_DEFAULT  RR_KILOBYTES(64)

typedef struct Rr_Arena Rr_Arena;
struct Rr_Arena
{
    uintptr_t Position;
    uintptr_t ReserveSize;
    uintptr_t CommitSize;
    uintptr_t Reserved;
    uintptr_t Commited;
};

extern Rr_Arena *Rr_CreateArena(size_t Reserve, size_t Commit);

extern Rr_Arena *Rr_CreateDefaultArena();

extern void Rr_ResetArena(Rr_Arena *Arena);

extern void Rr_DestroyArena(Rr_Arena *Arena);

extern void *Rr_AllocArena(Rr_Arena *Arena, size_t Size, size_t Align, size_t Count);

#define RR_ALLOC(Arena, Size)              Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_COUNT(Arena, Size, Count) Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, Count)

extern void Rr_PopArena(Rr_Arena *Arena, size_t Amount);

typedef struct Rr_ArenaScratch Rr_ArenaScratch;
struct Rr_ArenaScratch
{
    Rr_Arena *Arena;
    uintptr_t Position;
};

extern Rr_ArenaScratch Rr_CreateArenaScratch(Rr_Arena *Arena);

extern void Rr_DestroyArenaScratch(Rr_ArenaScratch Scratch);

extern void Rr_SetScratchTLS(void *TLSID);

extern void Rr_InitThreadScratch(size_t Size);

extern Rr_ArenaScratch Rr_GetArenaScratch(Rr_Arena *Conflict);

/*
 * Dynamic Slice
 */

extern void Rr_SliceGrow(void *Slice, size_t Size, Rr_Arena *Arena);

extern void Rr_SliceResize(void *Slice, size_t Size, size_t Count, Rr_Arena *Arena);

#define RR_SLICE_TYPE(Type) \
    struct                  \
    {                       \
        Type *Data;         \
        size_t Count;       \
        size_t Capacity;    \
    }

#define RR_SLICE_PUSH(Slice, Arena)                                                                             \
    ((Slice)->Count >= (Slice)->Capacity ? Rr_SliceGrow((Slice), sizeof(*(Slice)->Data), (Arena)), /* NOLINT */ \
     (Slice)->Data + (Slice)->Count++                                                                           \
                                         : (Slice)->Data + (Slice)->Count++)

#define RR_SLICE_POP(Slice) (Slice)->Count > 0 ? (Slice)->Count-- : (void)0

#define RR_SLICE_RESERVE(Slice, ElementCount, Arena)                               \
    ((Slice)->Capacity < (ElementCount) ? Rr_SliceResize(                          \
                                              (Slice),                             \
                                              sizeof(*(Slice)->Data), /* NOLINT */ \
                                              (ElementCount),                      \
                                              (Arena))                             \
                                        : (void)0)

#define RR_SLICE_EMPTY(Slice) (Slice)->Count = 0

#define RR_SLICE_DUPLICATE(Dst, Src, Arena)      \
    Rr_SliceReserve((Dst), (Src)->Count, Arena), \
        (Dst)->Count = (Src)->Count, SDL_memcpy((Dst)->Data, (Src)->Data, sizeof(*(Dst)->Data) * (Src)->Count)

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

extern void **Rr_MapUpsert(Rr_Map **Map, uintptr_t Key, Rr_Arena *Arena);

#ifdef __cplusplus
}
#endif
