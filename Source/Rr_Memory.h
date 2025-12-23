/*
 * Copyright (C) 2024-2026 Alexandr Semenov <tidesmain@gmail.com>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
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
                sizeof(*(Array)->Data) * (ElementCount),      \
                (Arena));                                     \
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

#define RR_RESET_ARRAY(Array, Arena)                        \
    do                                                      \
    {                                                       \
        if ((Array)->Capacity > 0)                          \
        {                                                   \
            (Array)->Data = RR_ALLOC_NO_ZERO(               \
                sizeof(*(Array)->Data) * (Array)->Capacity, \
                (Arena));                                   \
            (Array)->Count = 0;                             \
        }                                                   \
        else                                                \
        {                                                   \
            RR_ZERO_PTR((Array));                           \
        }                                                   \
    }                                                       \
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

#define RR_PTR_TO_OFFSET(Ptr, Offset) memcpy(&Ptr, &Offset, sizeof(uintptr_t))

#define RR_SERIALIZE_ARGS(Count, Ptr) \
    (void *)Ptr, Count, Dst_, &WriteOffset_, &Ptr

#define RR_DESERIALIZE_ARGS(Count, Ptr) (void *)Ptr, Count, Base, &Ptr

#define RR_SERIALIZE_PTR(Size, Ptr)                                        \
    {                                                                      \
        if (Dst_)                                                          \
        {                                                                  \
            memcpy(Dst_ + WriteOffset_, Ptr, Size);                        \
            memcpy(&Ptr, &WriteOffset_, sizeof(uintptr_t));                \
        }                                                                  \
        WriteOffset_ =                                                     \
            RR_ALIGN_POW2(WriteOffset_ + Size, (size_t)RR_SAFE_ALIGNMENT); \
    }

#define RR_BEGIN_SERIALIZE_FUNCTION(FunctionName, StructType) \
    size_t FunctionName(                                      \
        StructType *SrcPtr,                                   \
        size_t Count,                                         \
        void *DstPtr,                                         \
        size_t *DstOffsetPtr,                                 \
        void *WriteOffsetTo)                                  \
    {                                                         \
        bool RootStruct_ = true;                              \
        uintptr_t InitialOffset_ = 0;                         \
        if (DstOffsetPtr)                                     \
        {                                                     \
            InitialOffset_ = *DstOffsetPtr;                   \
            RootStruct_ = false;                              \
        }                                                     \
        size_t WriteOffset_ = InitialOffset_;                 \
        char *Dst_ = NULL;                                    \
        if (DstPtr)                                           \
        {                                                     \
            Dst_ = (char *)DstPtr;                            \
        }                                                     \
                                                              \
        StructType *Structs_ = SrcPtr;                        \
        if (Dst_)                                             \
        {                                                     \
            Structs_ = memcpy(                                \
                Dst_ + WriteOffset_,                          \
                SrcPtr,                                       \
                sizeof(StructType) * Count);                  \
        }                                                     \
        WriteOffset_ = RR_ALIGN_POW2(                         \
            WriteOffset_ + sizeof(StructType) * Count,        \
            (size_t)RR_SAFE_ALIGNMENT);                       \
                                                              \
        for (size_t Index_ = 0; Index_ < Count; ++Index_)     \
        {                                                     \
            StructType *Struct_ = Structs_ + Index_;

#define RR_END_SERIALIZE_FUNCTION()                                     \
    }                                                                   \
    if (DstOffsetPtr)                                                   \
    {                                                                   \
        *DstOffsetPtr = WriteOffset_;                                   \
    }                                                                   \
    if (WriteOffsetTo && Dst_ && !RootStruct_)                          \
    {                                                                   \
        memcpy(WriteOffsetTo, &InitialOffset_, sizeof(InitialOffset_)); \
    }                                                                   \
    return WriteOffset_ - InitialOffset_;                               \
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
            PtrValue = (uintptr_t)Base + AsOffset;         \
        }                                                  \
        memcpy((void *)&Ptr, &PtrValue, sizeof(PtrValue)); \
    }

#define RR_BEGIN_DESERIALIZE_FUNCTION(FunctionName, StructType)     \
    StructType *FunctionName(                                       \
        void *Data,                                                 \
        size_t Count,                                               \
        void *Base,                                                 \
        void *WritePtrTo)                                           \
    {                                                               \
        if (!Base)                                                  \
        {                                                           \
            Base = Data;                                            \
        }                                                           \
        StructType *Structs_;                                       \
        if (WritePtrTo)                                             \
        {                                                           \
            uintptr_t PtrValue = (uintptr_t)Base + (uintptr_t)Data; \
            memcpy(WritePtrTo, &PtrValue, sizeof(PtrValue));        \
            Structs_ = (void *)PtrValue;                            \
        }                                                           \
        else                                                        \
        {                                                           \
            Structs_ = Data;                                        \
        }                                                           \
                                                                    \
        for (size_t Index_ = 0; Index_ < Count; ++Index_)           \
        {                                                           \
            StructType *Struct_ = Structs_ + Index_;

#define RR_END_DESERIALIZE_FUNCTION() \
    }                                 \
    return Structs_;                  \
    }

#ifdef __cplusplus
}
#endif
