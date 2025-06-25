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

#include <Rr/Rr_Memory.h>

#include "Rr_Log.h"

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Platform.h>

#include <xxHash/xxhash.h>

#include <assert.h>
#include <limits.h>
#include <string.h>

void *Rr_Malloc(size_t Bytes)
{
    return malloc(Bytes);
}

void *Rr_Calloc(size_t Num, size_t Bytes)
{
    return calloc(Num, Bytes);
}

void *Rr_Realloc(void *Ptr, size_t Bytes)
{
    return realloc(Ptr, Bytes);
}

void Rr_Free(void *Ptr)
{
    free(Ptr);
}

Rr_Arena *Rr_CreateArena(size_t ReserveSize, size_t CommitSize)
{
    size_t PageSize = Rr_GetPlatformInfo()->PageSize;
    ReserveSize = RR_ALIGN_POW2(ReserveSize, PageSize);
    CommitSize = RR_ALIGN_POW2(CommitSize, PageSize);

    char *Data = Rr_ReserveMemory(ReserveSize);
    Rr_CommitMemory(Data, CommitSize);

    Rr_Arena *Arena = (Rr_Arena *)Data;
    *Arena = (Rr_Arena){
        .Position = sizeof(Rr_Arena),
        .ReserveSize = ReserveSize,
        .CommitSize = CommitSize,
        .Reserved = ReserveSize,
        .Commited = CommitSize,
    };

    return Arena;
}

Rr_Arena *Rr_CreateDefaultArena(void)
{
    return Rr_CreateArena(RR_ARENA_RESERVE_DEFAULT, RR_ARENA_COMMIT_DEFAULT);
}

void Rr_ResetArena(Rr_Arena *Arena)
{
    Arena->Position = sizeof(Rr_Arena);
}

void Rr_DestroyArena(Rr_Arena *Arena)
{
    if (Arena == NULL)
    {
        return;
    }
    Rr_ReleaseMemory((void *)Arena, Arena->ReserveSize);
}

Rr_Scratch Rr_CreateScratch(Rr_Arena *Arena)
{
    return (Rr_Scratch){ .Arena = Arena, .Position = Arena->Position };
}

void Rr_DestroyScratch(Rr_Scratch Scratch)
{
    Scratch.Arena->Position = Scratch.Position;
}

static _Thread_local Rr_Arena *ScratchArenas[2] = { 0 };

void Rr_CleanupScratchArena(void)
{
    for (size_t Index = 0; Index < 2; ++Index)
    {
        Rr_DestroyArena(ScratchArenas[Index]);
    }
}

void Rr_InitScratchArena(void)
{
    assert(
        ScratchArenas[0] == NULL &&
        "Scratch is already initialized for this thread!");
    for (size_t Index = 0; Index < 2; ++Index)
    {
        ScratchArenas[Index] = Rr_CreateDefaultArena();
    }
}

Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict)
{
    assert(
        ScratchArenas[0] != NULL && "Did you forget to call Rr_InitScratch()?");
    if (Conflict == NULL)
    {
        return Rr_CreateScratch(ScratchArenas[0]);
    }
    else
    {
        for (size_t Index = 0; Index < 2; ++Index)
        {
            if (ScratchArenas[Index] != Conflict)
            {
                return Rr_CreateScratch(ScratchArenas[Index]);
            }
        }
    }

    RR_ABORT("Couldn't find appropriate arena for a scratch!");

    return (Rr_Scratch){ 0 };
}

void *Rr_AllocArenaNoZero(
    Rr_Arena *Arena,
    size_t Size,
    size_t Align,
    size_t Count)
{
    if (Arena == NULL)
    {
        RR_ABORT("Allocating from NULL arena!");
    }

    if (Size == 0 || Count == 0)
    {
        RR_ABORT("Allocating 0 bytes from an arena is not allowed!");
    }

    size_t TotalSize = Size * Count;
    uintptr_t PositionAligned = RR_ALIGN_POW2(Arena->Position, Align);
    uintptr_t Target = PositionAligned + TotalSize;

    if (Arena->Commited < Target)
    {
        uintptr_t CommitTarget = Target + Arena->CommitSize - 1;
        CommitTarget -= CommitTarget % Arena->CommitSize;
        CommitTarget = RR_MIN(CommitTarget, Arena->Reserved);
        uintptr_t CommitSize = CommitTarget - Arena->Commited;
        char *CommitPtr = (char *)Arena + Arena->Commited;
        Rr_CommitMemory(CommitPtr, CommitSize);
        Arena->Commited = CommitTarget;
    }

    char *Result = NULL;
    if (Arena->Commited >= Target)
    {
        Result = (char *)Arena + PositionAligned;
        Arena->Position = Target;
    }
    else
    {
        RR_ABORT("Arena reserved memory overflow!");
    }

    return Result;
}

void *Rr_AllocArena(Rr_Arena *Arena, size_t Size, size_t Align, size_t Count)
{
    return memset(
        Rr_AllocArenaNoZero(Arena, Size, Align, Count),
        0,
        Size * Count);
}

void Rr_PopArena(Rr_Arena *Arena, size_t Amount)
{
    Arena->Position -= Amount;
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
    Data = RR_ALLOC_NO_ZERO(Arena, Size * Replica.Capacity);

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
    *Map = (Rr_Map *)RR_ALLOC(Arena, sizeof(Rr_Map));
    (*Map)->Key = Key;
    return &(*Map)->Value;
}

void Rr_AddHandleToSet(Rr_HandleSet *Set, Rr_Handle Handle, Rr_Arena *Arena)
{
    Rr_HandleTrie **Trie = &Set->Trie;
    if (*Trie != NULL)
    {
        for (uint64_t Hash = XXH64(&Handle, sizeof(Handle), 0); *Trie;
             Hash <<= 2)
        {
            if (Handle == (*Trie)->Handle)
            {
                return;
            }
            static const int Shift = (sizeof(Rr_MapKey) * CHAR_BIT) - 2;
            Trie = &(*Trie)->Children[Hash >> Shift];
        }
    }
    assert(Arena != NULL && "Arena is NULL!");
    *Trie = Rr_PushHandleTrieIntoHive(&Set->Hive, Arena).Element;
    RR_ZERO_PTR(*Trie);
    (*Trie)->Handle = Handle;
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
        Header = RR_ALLOC(Arena, Size + sizeof(Rr_FreeListHeader));
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
