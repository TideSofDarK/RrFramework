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

#include "Rr_Memory.h"

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Platform.h>

#if defined(__x86_64__) && !defined(__APPLE__)
#include <xxHash/xxh_x86dispatch.h>
#else
#include <xxHash/xxhash.h>
#endif

#include <assert.h>
#include <limits.h>
#include <string.h>

void *Rr_AlignedAlloc(size_t Size, size_t Alignment)
{
#ifdef _MSC_VER
    return _aligned_malloc(Size, Alignment);
#else
    return aligned_alloc(Alignment, Size);
#endif
}

void Rr_AlignedFree(void *Ptr)
{
#ifdef _MSC_VER
    _aligned_free(Ptr);
#else
    free(Ptr);
#endif
}

void Rr_GrowArray(void *Array, size_t Size, size_t MinCount, Rr_Arena *Arena)
{
    assert(Array != NULL && "Attempt to grow an array but it's NULL!");
    assert(Arena != NULL && "Attempt to grow an array but Arena is NULL!");

    RR_ARRAY(void) Replica;
    memcpy(&Replica, Array, sizeof(Replica));

    void *Data = NULL;
    Replica.Capacity = Replica.Capacity ? Replica.Capacity : 1;
    if (MinCount)
    {
        Replica.Capacity = RR_MAX(Replica.Capacity * 2, MinCount);
    }
    else
    {
        Replica.Capacity *= 2;
    }
    Data = RR_ALLOC_NO_ZERO(Size * Replica.Capacity, Arena);

    if (Replica.Count && Replica.Data)
    {
        memcpy(Data, Replica.Data, Size * Replica.Count);
    }
    Replica.Data = Data;

    memcpy(Array, &Replica, sizeof(Replica));
}

void **Rr_FindInHashTrie(Rr_HashTrie **Map, Rr_HashTrieKey Key, Rr_Arena *Arena)
{
    if (*Map != NULL)
    {
        for (Rr_HashTrieKey Hash = Key; *Map; Hash <<= 2)
        {
            if (Key == (*Map)->Key)
            {
                return &(*Map)->Value;
            }
            static const int Shift = (sizeof(Rr_HashTrieKey) * CHAR_BIT) - 2;
            Map = &(*Map)->Child[Hash >> Shift];
        }
    }
    if (Arena == NULL)
    {
        return NULL;
    }
    *Map = (Rr_HashTrie *)RR_ALLOC(sizeof(Rr_HashTrie), Arena);
    (*Map)->Key = Key;
    return &(*Map)->Value;
}

bool Rr_AddHandleToSet(Rr_HandleSet *Set, Rr_Handle Handle, Rr_Arena *Arena)
{
    Rr_HandleTrie **Trie = &Set->Trie;
    if (*Trie != NULL)
    {
        for (uint64_t Hash = XXH64(&Handle, sizeof(Handle), 0); *Trie;
             Hash <<= 2)
        {
            if (Handle == (*Trie)->Handle)
            {
                return false;
            }
            static const int Shift = (sizeof(Rr_HashTrieKey) * CHAR_BIT) - 2;
            Trie = &(*Trie)->Children[Hash >> Shift];
        }
    }
    assert(Arena != NULL && "Arena is NULL!");
    *Trie = Rr_PushHandleTrieIntoHive(&Set->Hive, Arena).Element;
    RR_ZERO((*Trie)->Children);
    (*Trie)->Handle = Handle;
    return true;
}

typedef struct Rr_FreeList Rr_FreeList;
struct Rr_FreeList
{
    void *First;
    void *Unused;
};

typedef struct Rr_FreeListHeader Rr_FreeListHeader;
struct Rr_FreeListHeader
{
    void *Data;
    Rr_FreeListHeader *Next;
};

void *Rr_GetFreeListItem(void *FreeList, size_t Size, Rr_Arena *Arena)
{
    assert(FreeList != NULL);

    Rr_FreeList *FreeListTyped = FreeList;
    Rr_FreeListHeader *Header = NULL;
    Rr_FreeListHeader *OldFirst = FreeListTyped->First;
    if (OldFirst == NULL)
    {
        Header = RR_ALLOC(Size + sizeof(Rr_FreeListHeader), Arena);
        Header->Data = ((char *)Header) + sizeof(Rr_FreeListHeader);
    }
    else
    {
        FreeListTyped->First = OldFirst->Next;
        Header = OldFirst;
    }

    return Header->Data;
}

void Rr_ReturnFreeListItem(void *FreeList, void *Pointer)
{
    assert(FreeList != NULL && Pointer != NULL);

    Rr_FreeList *FreeListTyped = FreeList;
    Rr_FreeListHeader *OldFirst = FreeListTyped->First;
    FreeListTyped->First = ((char *)Pointer) - sizeof(Rr_FreeListHeader);
    ((Rr_FreeListHeader *)FreeListTyped->First)->Next = OldFirst;
}
