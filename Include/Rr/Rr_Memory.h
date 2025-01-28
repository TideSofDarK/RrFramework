#pragma once

#include <Rr/Rr_Defines.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Common Memory Functions
 */

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

extern Rr_Arena *Rr_CreateDefaultArena(void);

extern void Rr_ResetArena(Rr_Arena *Arena);

extern void Rr_DestroyArena(Rr_Arena *Arena);

extern void *Rr_AllocArenaNoZero(Rr_Arena *Arena, size_t Size, size_t Align, size_t Count);

extern void *Rr_AllocArena(Rr_Arena *Arena, size_t Size, size_t Align, size_t Count);

#define RR_ALLOC(Arena, Size)              Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_NO_ZERO(Arena, Size)      Rr_AllocArenaNoZero(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_STRUCT(Arena, Struct)     (Struct *)Rr_AllocArena(Arena, sizeof(Struct), RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_COUNT(Arena, Size, Count) Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, Count)
#define RR_ALLOC_STRUCT_COUNT(Arena, Struct, Count) \
    (Struct *)Rr_AllocArena(Arena, sizeof(Struct), RR_SAFE_ALIGNMENT, Count)

extern void Rr_PopArena(Rr_Arena *Arena, size_t Amount);

typedef struct Rr_Scratch Rr_Scratch;
struct Rr_Scratch
{
    Rr_Arena *Arena;
    uintptr_t Position;
};

extern Rr_Scratch Rr_CreateScratch(Rr_Arena *Arena);

extern void Rr_DestroyScratch(Rr_Scratch Scratch);

extern void Rr_SetScratchTLS(void *TLSID);

extern void Rr_InitScratch(size_t Size);

extern Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict);

/*
 * Dynamic Slice
 */

extern void Rr_GrowSlice(void *Slice, size_t Size, Rr_Arena *Arena);

extern void Rr_ResizeSlice(void *Slice, size_t Size, size_t Count, Rr_Arena *Arena);

#define RR_SLICE(Type)   \
    struct               \
    {                    \
        Type *Data;      \
        size_t Count;    \
        size_t Capacity; \
    }

#define RR_PUSH_SLICE(Slice, Arena)                                                                             \
    ((Slice)->Count >= (Slice)->Capacity ? Rr_GrowSlice((Slice), sizeof(*(Slice)->Data), (Arena)), /* NOLINT */ \
     (Slice)->Data + (Slice)->Count++                                                                           \
                                         : (Slice)->Data + (Slice)->Count++)

#define RR_POP_SLICE(Slice) (Slice)->Count > 0 ? (Slice)->Count-- : (int)0

#define RR_RESERVE_SLICE(Slice, ElementCount, Arena)                               \
    ((Slice)->Capacity < (ElementCount) ? Rr_ResizeSlice(                          \
                                              (Slice),                             \
                                              sizeof(*(Slice)->Data), /* NOLINT */ \
                                              (ElementCount),                      \
                                              (Arena))                             \
                                        : (void)0)

#define RR_EMPTY_SLICE(Slice) (Slice)->Count = 0

#define RR_DUPLICATE_SLICE(Dst, Src, Arena)                                  \
    Rr_ResizeSlice((Dst), (Src)->Count, Arena), (Dst)->Count = (Src)->Count, \
                                                memcpy((Dst)->Data, (Src)->Data, sizeof(*(Dst)->Data) * (Src)->Count)

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

extern void **Rr_UpsertMap(Rr_Map **Map, uintptr_t Key, Rr_Arena *Arena);

#define RR_UPSERT(Map, Key, Arena) (void *)Rr_UpsertMap((Map), (uintptr_t)Key, Arena)

/*
 * Free List
 */

#define RR_FREE_LIST(Type) \
    struct                 \
    {                      \
        void *First;       \
        Type *SizeHint;    \
    }

extern void *Rr_GetFreeListItem(void *FreeList, size_t Size, Rr_Arena *Arena);

extern void Rr_ReturnFreeListItem(void *FreeList, void *Pointer);

#define RR_GET_FREE_LIST_ITEM(FreeList, Arena) Rr_GetFreeListItem((FreeList), sizeof(*(FreeList)->SizeHint), Arena)

#define RR_RETURN_FREE_LIST_ITEM(FreeList, Pointer) Rr_ReturnFreeListItem((FreeList), Pointer)

#ifdef __cplusplus
}
#endif
