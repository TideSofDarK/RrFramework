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

extern void *Rr_AllocArenaNoZero(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count);

extern void *Rr_AllocArena(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count);

#define RR_ALLOC(Arena, Size) Rr_AllocArena(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_NO_ZERO(Arena, Size) \
    Rr_AllocArenaNoZero(Arena, Size, RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_TYPE(Arena, Type) \
    (Type *)Rr_AllocArena(Arena, sizeof(Type), RR_SAFE_ALIGNMENT, 1)
#define RR_ALLOC_TYPE_COUNT(Arena, Type, Count) \
    (Type *)Rr_AllocArena(Arena, sizeof(Type), RR_SAFE_ALIGNMENT, Count)
#define RR_ALLOC_COPY(Arena, Dst, Src, Size) \
    Dst = RR_ALLOC_NO_ZERO(Arena, Size);     \
    memcpy(Dst, Src, Size)

extern void Rr_PopArena(Rr_Arena *Arena, size_t Amount);

static void *Rr_GenericArenaAlloc(void *Arena, size_t Size)
{
    return RR_ALLOC_NO_ZERO((Rr_Arena *)Arena, Size);
}

static void Rr_GenericArenaFree(void *Arena, void *Ptr)
{
}

/*
 * Scratch Arena
 */

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
 * Dynamic Array
 */

extern void Rr_GrowArray(void *Array, size_t Size, Rr_Arena *Arena);

extern void Rr_ReserveArray(
    void *Array,
    size_t Size,
    size_t Count,
    Rr_Arena *Arena);

#define RR_ARRAY(Type)   \
    struct               \
    {                    \
        Type *Data;      \
        size_t Count;    \
        size_t Capacity; \
    }

#define RR_PUSH_INTO_ARRAY(Array, Arena)                                    \
    ((Array)->Count >= (Array)->Capacity                                    \
     ? Rr_GrowArray((Array), sizeof(*(Array)->Data), (Arena)), /* NOLINT */ \
     memset((Array)->Data + (Array)->Count, 0, sizeof(*(Array)->Data)),     \
     (Array)->Data + (Array)->Count++                                       \
     : (Array)->Data + (Array)->Count++)

#define RR_POP_FROM_ARRAY(Array) \
    ((Array)->Count--, (Array)->Data[(Array)->Count])

#define RR_RESERVE_ARRAY(Array, ElementCount, Arena) \
    ((Array)->Capacity < (ElementCount)              \
         ? Rr_ReserveArray(                          \
               (Array),                              \
               sizeof(*(Array)->Data), /* NOLINT */  \
               (ElementCount),                       \
               (Arena))                              \
         : (void)0)

#define RR_RESET_ARRAY(Array, Arena)                               \
    ((Array)->Count > 0 ? Rr_ReserveArray(                         \
                              (Array),                             \
                              sizeof(*(Array)->Data), /* NOLINT */ \
                              (Array)->Capacity,                   \
                              (Arena))                             \
                        : (void)0),                                \
        (Array)->Count = 0

#define RR_EMPTY_ARRAY(Array) (Array)->Count = 0

#define RR_COPY_ARRAY(Dst, Src, Arena)          \
    Rr_ResizeArray((Dst), (Src)->Count, Arena), \
        (Dst)->Count = (Src)->Count,            \
        memcpy((Dst)->Data, (Src)->Data, sizeof(*(Dst)->Data) * (Src)->Count)

/*
 * Hashmap
 */

typedef uint64_t Rr_MapKey;

typedef struct Rr_Map Rr_Map;
struct Rr_Map
{
    Rr_Map *Child[4];
    Rr_MapKey Key;
    void *Value;
};

extern void **Rr_GetMapValue(Rr_Map **Map, Rr_MapKey Key, Rr_Arena *Arena);

#define RR_GET_MAP_VALUE(Map, Key, Arena) \
    ((void *)Rr_GetMapValue((Map), (uintptr_t)Key, Arena))

#define RR_GET_MAP_VALUE_DEREF(Map, Key, Arena) \
    (*(void **)Rr_GetMapValue((Map), (uintptr_t)Key, Arena))

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

#define RR_GET_FREE_LIST_ITEM(FreeList, Arena) \
    Rr_GetFreeListItem((FreeList), sizeof(*(FreeList)->SizeHint), Arena)

#define RR_RETURN_FREE_LIST_ITEM(FreeList, Pointer) \
    Rr_ReturnFreeListItem((FreeList), Pointer)

/*
 * Hive (aka Colony)
 *
 * Maintains a list of contiguous element groups.
 * Doesn't reallocate and therefore doesn't invalidate pointers.
 * Uses skiplists to skip erased elements during iteration.
 */

#define RR_HIVE_GROW    2
#define RR_HIVE_INITIAL 4

typedef uint16_t Rr_HiveSkipType;

#define RR_HIVE_GROUP(Type)       \
    struct                        \
    {                             \
        Rr_HiveSkipType *Skips;   \
        void *Next;               \
        Type *Elements;           \
        void *Previous;           \
        Rr_HiveSkipType Capacity; \
        Rr_HiveSkipType Count;    \
        void *NextFree;           \
        void *PreviousFree;       \
        size_t GroupNumber;       \
    }

#define RR_HIVE_ITERATOR(Type)       \
    struct                           \
    {                                \
        RR_HIVE_GROUP(Type) * Group; \
        Rr_HiveSkipType *Skip;       \
        Type *Element;               \
    }

#define RR_HIVE(Type)                 \
    struct                            \
    {                                 \
        size_t Count;                 \
        size_t Capacity;              \
        RR_HIVE_GROUP(Type) * First;  \
        RR_HIVE_GROUP(Type) * Last;   \
        RR_HIVE_ITERATOR(Type) Begin; \
        RR_HIVE_ITERATOR(Type) End;   \
        Type *PushedElement;          \
    }

extern void Rr_PushIntoHive(void *Hive, size_t ElementSize, Rr_Arena *Arena);

#define RR_PUSH_INTO_HIVE(Hive, Arena)                             \
    (Rr_PushIntoHive(Hive, sizeof(*(Hive)->PushedElement), Arena), \
     (Hive)->PushedElement)

#define RR_GET_HIVE_ITERATOR(Hive, It, ElementPtr)                          \
    do                                                                      \
    {                                                                       \
        if((Hive)->Last != NULL)                                            \
        {                                                                   \
            for(RR_HIVE_GROUP(void) *Group = (void *)(Hive)->Last;          \
                Group != NULL;                                              \
                Group = Group->Previous)                                    \
            {                                                               \
                if((char *)ElementPtr >= (char *)Group->Elements &&         \
                   (char *)ElementPtr < (char *)Group->Skips)               \
                {                                                           \
                    (It)->Group = (void *)Group;                            \
                    (It)->Skip = Group->Skips + ((char *)ElementPtr -       \
                                                 (char *)Group->Elements) / \
                                                    sizeof(*ElementPtr);    \
                    (It)->Element = ElementPtr;                             \
                }                                                           \
            }                                                               \
        }                                                                   \
    }                                                                       \
    while(0)

#define RR_BEGIN_HIVE_ITERATOR(Hive, It)    \
    It.Group = (void *)(Hive)->Begin.Group; \
    It.Skip = (Hive)->Begin.Skip;           \
    It.Element = (Hive)->Begin.Element;

#define RR_ADVANCE_HIVE_ITERATOR(Hive, It)                             \
    ((It)->Element += (1 + *(It)->Skip),                               \
     ((It)->Element - (It)->Group->Elements >= (It)->Group->Count)     \
         ? ((It)->Group = (void *)(It)->Group->Next,                   \
            ((It)->Group ? (It)->Element = (It)->Group->Elements : 0)) \
         : 0)

#define RR_FOR_EACH_IN_HIVE(Type, ItName, Hive)                        \
    for(RR_HIVE_ITERATOR(Type) ItName = { (void *)(Hive)->Begin.Group, \
                                          (Hive)->Begin.Skip,          \
                                          (Hive)->Begin.Element };     \
        It.Group != NULL;                                              \
        RR_ADVANCE_HIVE_ITERATOR((Hive), &ItName))

extern void Rr_TestHive(void);

#ifdef __cplusplus
}
#endif
