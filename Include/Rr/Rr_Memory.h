/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

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

extern void Rr_InitScratchArena(void);

extern void Rr_CleanupScratchArena(void);

extern Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict);

/*
 * Dynamic Array
 */

extern void Rr_GrowArray(
    void *Array,
    size_t Size,
    size_t MinCount,
    Rr_Arena *Arena);

#define RR_ARRAY(Type)   \
    struct               \
    {                    \
        Type *Data;      \
        size_t Count;    \
        size_t Capacity; \
    }

#define RR_RESERVE_ARRAY(Array, ElementCount, Arena)          \
    do                                                        \
    {                                                         \
        if ((Array)->Capacity < (ElementCount))               \
        {                                                     \
            void *OldData = (Array)->Data;                    \
            (Array)->Data = RR_ALLOC_NO_ZERO(                 \
                (Arena),                                      \
                sizeof(*(Array)->Data) * (ElementCount));     \
            (Array)->Capacity = (ElementCount);               \
            if ((Array)->Count > 0 && OldData)                \
            {                                                 \
                memcpy(                                       \
                    (Array)->Data,                            \
                    OldData,                                  \
                    sizeof(*(Array)->Data) * (Array)->Count); \
            }                                                 \
        }                                                     \
    }                                                         \
    while (0)

#define RR_PUSH_INTO_ARRAY(Array, Arena)                                       \
    ((Array)->Count >= (Array)->Capacity                                       \
     ? Rr_GrowArray((Array), sizeof(*(Array)->Data), 0, (Arena)), /* NOLINT */ \
     (Array)->Data + (Array)->Count++                                          \
     : (Array)->Data + (Array)->Count++)

#define RR_PUSH_INTO_ARRAY_MANY(Array, ElementCount, Arena) \
    (((Array)->Count + (ElementCount)) > (Array)->Capacity  \
         ? Rr_GrowArray(                                    \
               (Array),                                     \
               sizeof(*(Array)->Data),                      \
               (Array)->Count + (ElementCount),             \
               (Arena)) /* NOLINT */                        \
         : (void)0,                                         \
     (Array)->Count += (ElementCount),                      \
     (Array)->Data + ((Array)->Count - (ElementCount)))

#define RR_POP_FROM_ARRAY(Array) \
    ((Array)->Count--, (Array)->Data[(Array)->Count])

#define RR_RESET_ARRAY(Array, Arena)                         \
    do                                                       \
    {                                                        \
        if ((Array)->Capacity > 0)                           \
        {                                                    \
            (Array)->Data = RR_ALLOC_NO_ZERO(                \
                (Arena),                                     \
                sizeof(*(Array)->Data) * (Array)->Capacity); \
            (Array)->Count = 0;                              \
        }                                                    \
        else                                                 \
        {                                                    \
            RR_ZERO_PTR((Array));                            \
        }                                                    \
    }                                                        \
    while (0)

#define RR_CLEAR_ARRAY(Array) (Array)->Count = 0

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

#ifdef __cplusplus
}
#endif
