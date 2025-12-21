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

#include <Rr/Rr_Arena.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#define RR_LAST_ARRAY_ELEMENT(Array) ((Array)->Data[(Array)->Count - 1])

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
 * Handle Hive
 */

typedef void *Rr_Handle;

#define RR_HIVE_TYPE      Rr_Handle
#define RR_HIVE_TYPE_NAME Handle
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

/*
 * Handle Set
 */

typedef struct Rr_HandleTrie Rr_HandleTrie;
struct Rr_HandleTrie
{
    void *Handle;
    Rr_HandleTrie *Children[4];
};

#define RR_HIVE_TYPE      Rr_HandleTrie
#define RR_HIVE_TYPE_NAME HandleTrie
#define RR_HIVE_PREFIX    Rr_
#include "Rr_Hive.h"

typedef struct Rr_HandleSet Rr_HandleSet;
struct Rr_HandleSet
{
    Rr_HandleTrie *Trie;
    Rr_HandleTrieHive Hive;
};

extern bool Rr_AddHandleToSet(
    Rr_HandleSet *Set,
    Rr_Handle Handle,
    Rr_Arena *Arena);

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

extern void *Rr_NextFreeListItem(void *Pointer);

#define RR_GET_FREE_LIST_ITEM(FreeList, Arena) \
    Rr_GetFreeListItem((FreeList), sizeof(*(FreeList)->SizeHint), Arena)

#define RR_RETURN_FREE_LIST_ITEM(FreeList, Pointer) \
    Rr_ReturnFreeListItem((FreeList), Pointer)

#define RR_NEXT_FREE_LIST_ITEM(Pointer) Rr_NextFreeListItem((Pointer))

/*
 * Generic Serialization
 */

#define RR_SERIALIZE(Amount, Src)                                             \
    {                                                                         \
        if (OutData)                                                          \
        {                                                                     \
            memcpy(((char *)(OutData)) + Current, (Src), Amount);             \
        }                                                                     \
        Current = RR_ALIGN_POW2(Current + Amount, (size_t)RR_SAFE_ALIGNMENT); \
    }

#define RR_SERIALIZE_PTR(Amount, Src)                                         \
    {                                                                         \
        if (OutData)                                                          \
        {                                                                     \
            memcpy(((char *)(OutData)) + Current, (Src), Amount);             \
            memcpy((void *)&Src, &Current, sizeof(uintptr_t));                \
        }                                                                     \
        Current = RR_ALIGN_POW2(Current + Amount, (size_t)RR_SAFE_ALIGNMENT); \
    }

#define RR_SERIALIZE_STRUCT_PTR(Amount, Src, OutStructPtr)                    \
    {                                                                         \
        if (OutData)                                                          \
        {                                                                     \
            OutStructPtr =                                                    \
                memcpy(((char *)(OutData)) + Current, (Src), Amount);         \
            memcpy((void *)&Src, &Current, sizeof(uintptr_t));                \
        }                                                                     \
        else                                                                  \
        {                                                                     \
            OutStructPtr = (void *)Src;                                       \
        }                                                                     \
        Current = RR_ALIGN_POW2(Current + Amount, (size_t)RR_SAFE_ALIGNMENT); \
    }

#define RR_BEGIN_SERIALIZE_FUNCTION(FunctionName, StructType) \
    StructType *FunctionName(                                 \
        StructType const *Struct,                             \
        size_t *OutSize,                                      \
        void *OutData)                                        \
    {                                                         \
        if (!OutSize)                                         \
        {                                                     \
            return;                                           \
        }                                                     \
        size_t Current = 0;                                   \
        if (OutData)                                          \
        {                                                     \
            FunctionName(Struct, &Current, NULL);             \
            assert(*OutSize == Current);                      \
            Current = 0;                                      \
            RR_SERIALIZE(sizeof(StructType), Struct);         \
            Struct = OutData;                                 \
        }                                                     \
        else                                                  \
        {                                                     \
            RR_SERIALIZE(sizeof(StructType), Struct);         \
        }

#define RR_END_SERIALIZE_FUNCTION() \
    *OutSize = Current;             \
    return Struct;                  \
    }

#define RR_DESERIALIZE_PTR(Ptr)                            \
    {                                                      \
        uintptr_t AsOffset = (uintptr_t)(Ptr);             \
        uintptr_t PtrValue;                                \
        if (AsOffset == 0)                                 \
        {                                                  \
            PtrValue = (uintptr_t)NULL;                    \
        }                                                  \
        else                                               \
        {                                                  \
            PtrValue = (uintptr_t)Data + AsOffset;         \
        }                                                  \
        memcpy((void *)&Ptr, &PtrValue, sizeof(PtrValue)); \
    }

#define RR_BEGIN_DESERIALIZE_FUNCTION(FunctionName, StructType) \
    StructType *FunctionName(void *Data)                        \
    {                                                           \
        StructType *Struct = Data;

#define RR_END_DESERIALIZE_FUNCTION() \
    return Struct;                    \
    }

#ifdef __cplusplus
}
#endif
