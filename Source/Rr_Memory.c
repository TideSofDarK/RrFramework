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

void **Rr_GetMapValue(Rr_Map **Map, Rr_MapKey Key, Rr_Arena *Arena)
{
    if (*Map != NULL)
    {
        for (Rr_MapKey Hash = Key; *Map; Hash <<= 2)
        {
            if (Key == (*Map)->Key)
            {
                return &(*Map)->Value;
            }
            static const int Shift = (sizeof(Rr_MapKey) * CHAR_BIT) - 2;
            Map = &(*Map)->Child[Hash >> Shift];
        }
    }
    if (Arena == NULL)
    {
        return NULL;
    }
    *Map = (Rr_Map *)RR_ALLOC(sizeof(Rr_Map), Arena);
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
            static const int Shift = (sizeof(Rr_MapKey) * CHAR_BIT) - 2;
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
