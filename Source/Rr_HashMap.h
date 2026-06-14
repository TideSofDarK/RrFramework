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

#ifndef RR_HASH_MAP_H
#define RR_HASH_MAP_H

#include "Rr_Hash.h"

#include <Rr/Rr_Thread.h>

#include <stdio.h>
#include <string.h>

#if defined(__BMI__)
#include <immintrin.h>
#elif defined(RR_MSVC)
#include <intrin.h>
#endif

static inline uint64_t Rr_HashMapMod(uint64_t Hash, uint64_t Count)
{
    uint64_t Result;
#if defined(RR_MSVC_X86)
    (void)_umul128(Hash, Count, &Result);
#elif defined(RR_MSVC_ARM)
    Result = _umulh(Hash, Count);
#else
    Result = (uint64_t)(((__uint128_t)Hash * Count) >> 64);
#endif

    return Result;
}

static inline uint64_t Rr_HashMapLog2(uint64_t Value)
{
    uint64_t Result;
#if defined(__BMI__)
    Result = (uint64_t)_lzcnt_u64(Value);
#elif defined(RR_MSVC_X86)
    Result = (uint64_t)__lzcnt64(Value);
#elif defined(RR_MSVC_ARM)
    if (_BitScanReverse64((unsigned long *)&Result, Value))
    {
        Result ^= 63;
    }
    else
    {
        Result = 64;
    }
#else
    Result = (uint64_t)__builtin_clzll((unsigned long long)Value);
#endif

    return 63 - Result;
}

#define RR_HASH_MAP_CONCAT(A, B)        A##B
#define RR_HASH_MAP_EXPAND_CONCAT(A, B) RR_HASH_MAP_CONCAT(A, B)

#endif

#ifndef RR_HASH_MAP_PREFIX
#define RR_HASH_MAP_PREFIX
#endif

#ifndef RR_HASH_MAP_KEY_TYPE
#error RR_HASH_MAP_KEY_TYPE is not set!
#define RR_HASH_MAP_KEY_TYPE int
#endif

#ifndef RR_HASH_MAP_NAME
#error RR_HASH_MAP_NAME
#endif

#ifndef RR_HASH_MAP_INITIAL_CAPACITY
#define RR_HASH_MAP_INITIAL_CAPACITY 8
#endif

#define RR_HASH_MAP_STEPS Rr_HashMapLog2(RR_HASH_MAP_INITIAL_CAPACITY) - 1U

#ifndef RR_HASH_MAP_LOAD_FACTOR
#define RR_HASH_MAP_LOAD_FACTOR 0.75
#endif

#define RR_HASH_MAP_MAP_TYPE \
    RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_PREFIX, RR_HASH_MAP_NAME)

#define RR_HASH_MAP_BUCKET_TYPE \
    RR_HASH_MAP_EXPAND_CONCAT(  \
        RR_HASH_MAP_PREFIX,     \
        RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_NAME, Bucket))

typedef struct RR_HASH_MAP_BUCKET_TYPE RR_HASH_MAP_BUCKET_TYPE;
struct RR_HASH_MAP_BUCKET_TYPE
{
#if defined(RR_HASH_MAP_VALUE_TYPE)
    RR_HASH_MAP_VALUE_TYPE Value;
#endif
    RR_HASH_MAP_KEY_TYPE Key;
    uint64_t Hash;
    bool Occupied;
    bool Reusable;
};

#define RR_HASH_MAP_NODE_TYPE  \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_NAME, Node))

typedef struct RR_HASH_MAP_NODE_TYPE RR_HASH_MAP_NODE_TYPE;
struct RR_HASH_MAP_NODE_TYPE
{
    struct RR_HASH_MAP_MAP_TYPE *Map;
    size_t Capacity;
    RR_HASH_MAP_NODE_TYPE *Next;
    RR_HASH_MAP_BUCKET_TYPE Buckets[];
};

#define RR_HASH_MAP_ITERATOR_TYPE \
    RR_HASH_MAP_EXPAND_CONCAT(    \
        RR_HASH_MAP_PREFIX,       \
        RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_NAME, Iterator))

typedef struct RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_ITERATOR_TYPE;
struct RR_HASH_MAP_ITERATOR_TYPE
{
    RR_HASH_MAP_BUCKET_TYPE *Data;
    RR_HASH_MAP_NODE_TYPE *Node;
};

typedef struct RR_HASH_MAP_MAP_TYPE RR_HASH_MAP_MAP_TYPE;
struct RR_HASH_MAP_MAP_TYPE
{
    RR_HASH_MAP_NODE_TYPE *First;
    RR_HASH_MAP_NODE_TYPE *Last;
    size_t Count;
    size_t Capacity;
};

#define RR_HASH_MAP_REHASH_NAME \
    RR_HASH_MAP_EXPAND_CONCAT(  \
        RR_HASH_MAP_PREFIX,     \
        RR_HASH_MAP_EXPAND_CONCAT(Rehash, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_REHASH_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    size_t Steps,
    Rr_Arena *Arena);

#define RR_HASH_MAP_INIT_NAME  \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(Init, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_INIT_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    Rr_Arena *Arena)
{
    size_t NodeSize =
        sizeof(RR_HASH_MAP_NODE_TYPE) +
        sizeof(RR_HASH_MAP_BUCKET_TYPE) * RR_HASH_MAP_INITIAL_CAPACITY;
    Map->First = Rr_Alloc(NodeSize, Arena);
    Map->First->Map = Map;
    Map->First->Capacity = RR_HASH_MAP_INITIAL_CAPACITY;
    Map->Last = Map->First;
    Map->Capacity = Map->First->Capacity;
}

#define RR_HASH_MAP_BEGIN_NAME \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(BeginIn, RR_HASH_MAP_NAME))

static inline RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_BEGIN_NAME(
    RR_HASH_MAP_MAP_TYPE *Map)
{
    RR_HASH_MAP_NODE_TYPE *Node = Map->First;
    size_t Index = 0;
    while (Node)
    {
        for (; Index < Node->Capacity; ++Index)
        {
            RR_HASH_MAP_BUCKET_TYPE *Bucket = &Node->Buckets[Index];
            if (Bucket->Occupied)
            {
                return (RR_HASH_MAP_ITERATOR_TYPE){
                    .Data = Bucket,
                    .Node = Node,
                };
            }
        }
        if (Node->Next)
        {
            Node = Node->Next;
            Index = 0;
        }
        else
        {
            break;
        }
    }

    return (RR_HASH_MAP_ITERATOR_TYPE){
        .Data = &Map->Last->Buckets[Map->Last->Capacity],
        .Node = Map->Last,
    };
}

#define RR_HASH_MAP_IS_END_NAME    \
    RR_HASH_MAP_EXPAND_CONCAT(     \
        RR_HASH_MAP_PREFIX,        \
        RR_HASH_MAP_EXPAND_CONCAT( \
            Is,                    \
            RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_NAME, End)))

static inline bool RR_HASH_MAP_IS_END_NAME(RR_HASH_MAP_ITERATOR_TYPE *It)
{
    RR_HASH_MAP_NODE_TYPE *Node = It->Node;
    RR_HASH_MAP_MAP_TYPE *Map = Node->Map;
    size_t IndexInNode = (size_t)(It->Data - Node->Buckets);

    return It->Node == Map->Last && IndexInNode == Node->Capacity;
}

#define RR_HASH_MAP_NEXT_NAME  \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(NextIn, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_NEXT_NAME(RR_HASH_MAP_ITERATOR_TYPE *It)
{
    RR_HASH_MAP_NODE_TYPE *Node = It->Node;
    RR_HASH_MAP_MAP_TYPE *Map = Node->Map;
    size_t IndexInNode = (size_t)(It->Data - Node->Buckets);
    size_t Index = IndexInNode + 1;
    while (Node)
    {
        for (; Index < Node->Capacity; ++Index)
        {
            RR_HASH_MAP_BUCKET_TYPE *Bucket = &Node->Buckets[Index];
            if (Bucket->Occupied)
            {
                It->Data = Bucket;
                It->Node = Node;

                return;
            }
        }
        if (Node->Next)
        {
            Node = Node->Next;
            Index = 0;
        }
        else
        {
            break;
        }
    }

    It->Data = &Map->Last->Buckets[Map->Last->Capacity];
    It->Node = Map->Last;
}

#define RR_HASH_MAP_ERASE_NAME \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(EraseFrom, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_ERASE_NAME(RR_HASH_MAP_ITERATOR_TYPE *It)
{
    It->Data->Occupied = false;
    It->Data->Reusable = true;
    RR_HASH_MAP_NODE_TYPE *Node = It->Node;
    RR_HASH_MAP_MAP_TYPE *Map = Node->Map;
    Map->Count--;

    RR_HASH_MAP_NEXT_NAME(It);
}

#define RR_HASH_MAP_RESERVE_NAME \
    RR_HASH_MAP_EXPAND_CONCAT(   \
        RR_HASH_MAP_PREFIX,      \
        RR_HASH_MAP_EXPAND_CONCAT(Reserve, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_RESERVE_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    size_t Size,
    Rr_Arena *Arena)
{
    if (Size == 0)
    {
        return;
    }

    double Load = (double)Size / (double)Map->Capacity;
    if (Load >= RR_HASH_MAP_LOAD_FACTOR)
    {
        size_t Capacity = Map->Capacity;
        size_t Steps = 0;
        while (((double)Capacity * RR_HASH_MAP_LOAD_FACTOR) < (double)Size)
        {
            Capacity *= 2;
            Steps++;
        }
        RR_HASH_MAP_REHASH_NAME(Map, Steps, Arena);
    }
}

#define RR_HASH_MAP_FIND_BUCKET_NAME \
    RR_HASH_MAP_EXPAND_CONCAT(       \
        RR_HASH_MAP_PREFIX,          \
        RR_HASH_MAP_EXPAND_CONCAT(FindBucketIn, RR_HASH_MAP_NAME))

static inline RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_FIND_BUCKET_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    size_t BucketIndex)
{
    size_t Steps;
    if (BucketIndex == 0)
    {
        Steps = 0;
    }
    else
    {
        Steps = Rr_HashMapLog2(BucketIndex);
        if (RR_HASH_MAP_STEPS > Steps)
        {
            Steps -= Steps;
        }
        else
        {
            Steps -= RR_HASH_MAP_STEPS;
        }
        /* Steps -= RR_MIN(Steps, RR_HASH_MAP_STEPS); */
    }
    RR_HASH_MAP_NODE_TYPE *Node = Map->First;
    for (size_t Index = 0; Index < Steps; ++Index)
    {
        Node = Node->Next;
    }
    size_t TotalCapacity = Map->First->Capacity << ((int)Steps - 1);
    size_t LocalBucketIndex = BucketIndex - TotalCapacity;

    return (RR_HASH_MAP_ITERATOR_TYPE){
        .Data = &Node->Buckets[LocalBucketIndex],
        .Node = Node,
    };
}

#define RR_HASH_MAP_INSERT_WITH_HASH_NAME \
    RR_HASH_MAP_EXPAND_CONCAT(            \
        RR_HASH_MAP_PREFIX,               \
        RR_HASH_MAP_EXPAND_CONCAT(        \
            InsertInto,                   \
            RR_HASH_MAP_EXPAND_CONCAT(RR_HASH_MAP_NAME, WithHash)))

static inline RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_INSERT_WITH_HASH_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    RR_HASH_MAP_KEY_TYPE const *Key,
#if defined(RR_HASH_MAP_VALUE_TYPE)
    RR_HASH_MAP_VALUE_TYPE const *Value,
#endif
    uint64_t Hash,
    Rr_Arena *Arena)
{
    double Load = (double)Map->Count / (double)Map->Capacity;
    if (Load >= RR_HASH_MAP_LOAD_FACTOR)
    {
        RR_HASH_MAP_REHASH_NAME(Map, 1, Arena);
    }

    size_t Mod = Rr_HashMapMod(Hash, Map->Capacity);
    size_t QuadraticProber = 1;

    RR_HASH_MAP_BUCKET_TYPE *Bucket;
    while (true)
    {
        RR_HASH_MAP_ITERATOR_TYPE It = RR_HASH_MAP_FIND_BUCKET_NAME(Map, Mod);
        Bucket = It.Data;
        if (Bucket->Occupied)
        {
            bool EqualHash = Hash == Bucket->Hash;
            bool EqualKey = EqualHash &&
#ifdef RR_HASH_MAP_COMPARE_NAME
                            RR_HASH_MAP_COMPARE_NAME(Key, &Bucket->Key)
#else
                            memcmp(
                                Key,
                                &Bucket->Key,
                                sizeof(RR_HASH_MAP_KEY_TYPE)) == 0
#endif
                ;
            if (EqualKey)
            {
#if defined(RR_HASH_MAP_VALUE_TYPE)
                Bucket->Value = *Value;
#endif

                return It;
            }
        }
        else
        {
#if defined(RR_HASH_MAP_VALUE_TYPE)
            Bucket->Value = *Value;
#endif
            Bucket->Key = *Key;
            Bucket->Hash = Hash;
            Bucket->Occupied = true;
            Bucket->Reusable = false;
            Map->Count++;

            return It;
        }

        Mod = (Mod + QuadraticProber * QuadraticProber) % Map->Capacity;
        QuadraticProber++;
    }
}

#define RR_HASH_MAP_INSERT_NAME \
    RR_HASH_MAP_EXPAND_CONCAT(  \
        RR_HASH_MAP_PREFIX,     \
        RR_HASH_MAP_EXPAND_CONCAT(InsertInto, RR_HASH_MAP_NAME))

static inline RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_INSERT_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    RR_HASH_MAP_KEY_TYPE const *Key,
#if defined(RR_HASH_MAP_VALUE_TYPE)
    RR_HASH_MAP_VALUE_TYPE const *Value,
#endif
    Rr_Arena *Arena)
{
    uint64_t Hash = Rr_Hash64(sizeof(*Key), Key);

    return RR_HASH_MAP_INSERT_WITH_HASH_NAME(
        Map,
        Key,
#if defined(RR_HASH_MAP_VALUE_TYPE)
        Value,
#endif
        Hash,
        Arena);
}

#define RR_HASH_MAP_FIND_NAME  \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(FindIn, RR_HASH_MAP_NAME))

static inline RR_HASH_MAP_ITERATOR_TYPE RR_HASH_MAP_FIND_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    RR_HASH_MAP_KEY_TYPE const *Key)
{
    uint64_t Hash = Rr_Hash64(sizeof(*Key), Key);
    size_t Mod = Rr_HashMapMod(Hash, Map->Capacity);
    size_t QuadraticProber = 1;

    RR_HASH_MAP_BUCKET_TYPE *Bucket;
    while (true)
    {
        RR_HASH_MAP_ITERATOR_TYPE It = RR_HASH_MAP_FIND_BUCKET_NAME(Map, Mod);
        Bucket = It.Data;
        if (Bucket->Reusable)
        {
        }
        else if (Bucket->Occupied)
        {
            bool EqualHash = Hash == Bucket->Hash;
            bool EqualKey = EqualHash &&
#ifdef RR_HASH_MAP_COMPARE_NAME
                            RR_HASH_MAP_COMPARE_NAME(Key, &Bucket->Key)
#else
                            memcmp(
                                Key,
                                &Bucket->Key,
                                sizeof(RR_HASH_MAP_KEY_TYPE)) == 0
#endif
                ;
            if (EqualKey)
            {
                return It;
            }
        }
        else
        {
            return (RR_HASH_MAP_ITERATOR_TYPE){
                .Data = &Map->Last->Buckets[Map->Last->Capacity],
                .Node = Map->Last,
            };
        }

        Mod = (Mod + QuadraticProber * QuadraticProber) % Map->Capacity;
        QuadraticProber++;
    }
}

static inline void RR_HASH_MAP_REHASH_NAME(
    RR_HASH_MAP_MAP_TYPE *Map,
    size_t Steps,
    Rr_Arena *Arena)
{
    Rr_Scratch Scratch = Rr_GetScratch(Arena);

    typedef struct TempBucket TempBucket;
    struct TempBucket
    {
#if defined(RR_HASH_MAP_VALUE_TYPE)
        RR_HASH_MAP_VALUE_TYPE Value;
#endif
        RR_HASH_MAP_KEY_TYPE Key;
        uint64_t Hash;
    };

    TempBucket *OldBuckets =
        Rr_AllocNoZero(sizeof(TempBucket) * Map->Count, Scratch.Arena);
    size_t OldBucketIndex = 0;

    RR_HASH_MAP_NODE_TYPE *Node = Map->First;
    while (Node)
    {
        for (size_t Index = 0; Index < Node->Capacity; ++Index)
        {
            RR_HASH_MAP_BUCKET_TYPE *Bucket = &Node->Buckets[Index];
            if (Bucket->Occupied)
            {
                OldBuckets[OldBucketIndex++] = (TempBucket){
#if defined(RR_HASH_MAP_VALUE_TYPE)
                    .Value = Bucket->Value,
#endif
                    .Key = Bucket->Key,
                    .Hash = Bucket->Hash,
                };
                ;
                Bucket->Occupied = false;
                Bucket->Reusable = false;
            }
        }

        Node = Node->Next;
    }

    for (size_t Index = 0; Index < Steps; ++Index)
    {
        size_t NodeSize = sizeof(RR_HASH_MAP_NODE_TYPE) +
                          sizeof(RR_HASH_MAP_BUCKET_TYPE) * Map->Capacity;
        RR_HASH_MAP_NODE_TYPE *NewNode = Rr_Alloc(NodeSize, Arena);
        NewNode->Map = Map;
        NewNode->Capacity = Map->Capacity;
        Map->Last->Next = NewNode;
        Map->Last = NewNode;
        Map->Capacity *= 2;
    }
    Map->Count = 0;

    for (size_t Index = 0; Index < OldBucketIndex; ++Index)
    {
        TempBucket *Bucket = &OldBuckets[Index];

        RR_HASH_MAP_INSERT_WITH_HASH_NAME(
            Map,
            &Bucket->Key,
#if defined(RR_HASH_MAP_VALUE_TYPE)
            &Bucket->Value,
#endif
            Bucket->Hash,
            NULL);
    }

    Rr_DestroyScratch(Scratch);
}

#define RR_HASH_MAP_CLEAR_NAME \
    RR_HASH_MAP_EXPAND_CONCAT( \
        RR_HASH_MAP_PREFIX,    \
        RR_HASH_MAP_EXPAND_CONCAT(Clear, RR_HASH_MAP_NAME))

static inline void RR_HASH_MAP_CLEAR_NAME(RR_HASH_MAP_MAP_TYPE *Map)
{
    RR_HASH_MAP_NODE_TYPE *Node = Map->First;
    while (Node)
    {
        memset(Node->Buckets, 0, sizeof(*Node->Buckets) * Node->Capacity);
        Node = Node->Next;
    }
    Map->Count = 0;
}

#undef RR_HASH_MAP_PREFIX
#undef RR_HASH_MAP_KEY_TYPE
#undef RR_HASH_MAP_VALUE_TYPE
#undef RR_HASH_MAP_NAME
#undef RR_HASH_MAP_INITIAL_CAPACITY
#undef RR_HASH_MAP_STEPS
#undef RR_HASH_MAP_LOAD_FACTOR
#undef RR_HASH_MAP_COMPARE_NAME
#undef RR_HASH_MAP_MAP_TYPE
#undef RR_HASH_MAP_BUCKET_TYPE
#undef RR_HASH_MAP_ITERATOR_TYPE
#undef RR_HASH_MAP_NODE_TYPE
#undef RR_HASH_MAP_MAP_TYPE
#undef RR_HASH_MAP_REHASH_NAME
#undef RR_HASH_MAP_INIT_NAME
#undef RR_HASH_MAP_BEGIN_NAME
#undef RR_HASH_MAP_IS_END_NAME
#undef RR_HASH_MAP_NEXT_NAME
#undef RR_HASH_MAP_ERASE_NAME
#undef RR_HASH_MAP_RESERVE_NAME
#undef RR_HASH_MAP_FIND_BUCKET_NAME
#undef RR_HASH_MAP_INSERT_WITH_HASH_NAME

#undef RR_HASH_MAP_FIND_NAME
#undef RR_HASH_MAP_REHASH_NAME
#undef RR_HASH_MAP_CLEAR_NAME
