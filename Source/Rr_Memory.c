#include "Rr_Memory.h"

#include "Rr_Log.h"

#include <Rr/Rr_Math.h>
#include <Rr/Rr_Platform.h>

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_thread.h>

#include <assert.h>
#include <limits.h>

void *Rr_Malloc(size_t Bytes)
{
    return SDL_malloc(Bytes);
}

void *Rr_Calloc(size_t Num, size_t Bytes)
{
    return SDL_calloc(Num, Bytes);
}

void *Rr_Realloc(void *Ptr, size_t Bytes)
{
    return SDL_realloc(Ptr, Bytes);
}

void Rr_Free(void *Ptr)
{
    SDL_free(Ptr);
}

void *Rr_AlignedAlloc(size_t Alignment, size_t Bytes)
{
    return SDL_aligned_alloc(Alignment, Bytes);
}

void Rr_AlignedFree(void *Ptr)
{
    SDL_aligned_free(Ptr);
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
    if(Arena == NULL)
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

static void SDLCALL Rr_CleanupScratchArena(void *ScratchArena)
{
    Rr_Arena **Arenas = ScratchArena;
    for(size_t Index = 0; Index < 2; ++Index)
    {
        Rr_DestroyArena(Arenas[Index]);
    }
    Rr_Free(ScratchArena);
}

static SDL_TLSID ScratchArenaTLS;

void Rr_SetScratchTLS(void *TLSID)
{
    ScratchArenaTLS = *((SDL_TLSID *)TLSID);
}

void Rr_InitScratch(size_t Size)
{
    if(SDL_GetTLS(&ScratchArenaTLS) != 0)
    {
        RR_ABORT("Scratch is already initialized for this thread!");
    }
    Rr_Arena **Arenas = Rr_Calloc(2, sizeof(Rr_Arena *));
    for(size_t Index = 0; Index < 2; ++Index)
    {
        Arenas[Index] = Rr_CreateDefaultArena();
    }
    SDL_SetTLS(&ScratchArenaTLS, Arenas, Rr_CleanupScratchArena);
}

Rr_Scratch Rr_GetScratch(Rr_Arena *Conflict)
{
    if(ScratchArenaTLS.value == 0)
    {
        RR_ABORT("ScratchArenaTLS is not set!");
    }
    Rr_Arena **Arenas = (Rr_Arena **)SDL_GetTLS(&ScratchArenaTLS);
    if(Conflict == NULL)
    {
        return Rr_CreateScratch(Arenas[0]);
    }
    else
    {
        for(size_t Index = 0; Index < 2; ++Index)
        {
            if(Arenas[Index] != Conflict)
            {
                return Rr_CreateScratch(Arenas[Index]);
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
    if(Arena == NULL)
    {
        RR_ABORT("Allocating from NULL arena!");
    }

    if(Size == 0 || Count == 0)
    {
        RR_ABORT("Allocating 0 bytes from an arena is not allowed!");
    }

    size_t TotalSize = Size * Count;
    uintptr_t PositionAligned = RR_ALIGN_POW2(Arena->Position, Align);
    uintptr_t Target = PositionAligned + TotalSize;

    if(Arena->Commited < Target)
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
    if(Arena->Commited >= Target)
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

Rr_SyncArena Rr_CreateSyncArena(void)
{
    return (Rr_SyncArena){ .Arena = Rr_CreateDefaultArena() };
}

void Rr_DestroySyncArena(Rr_SyncArena *Arena)
{
    Rr_DestroyArena(Arena->Arena);
}

void Rr_GrowArray(void *Array, size_t Size, Rr_Arena *Arena)
{
    assert(Array != NULL && "Attempt to grow an array but it's NULL!");
    assert(Arena != NULL && "Attempt to grow an array but Arena is NULL!");

    RR_ARRAY(void) Replica;
    memcpy(&Replica, Array, sizeof(Replica));

    if(Replica.Data != NULL &&
       (uintptr_t)Replica.Data + Replica.Capacity * Size ==
           (uintptr_t)Arena + Arena->Position)
    {
        /* Fast path: the array is at the tip of an arena.
         * Works best with nicely aligned sizes. */

        RR_ALLOC_NO_ZERO(Arena, Size * Replica.Capacity);
        Replica.Capacity *= 2;
    }
    else
    {
        void *Data = NULL;
        Replica.Capacity = Replica.Capacity ? Replica.Capacity : 1;
        Replica.Capacity *= 2;
        Data = RR_ALLOC_NO_ZERO(Arena, Size * Replica.Capacity);

        if(Replica.Count)
        {
            memcpy(Data, Replica.Data, Size * Replica.Count);
        }
        Replica.Data = Data;
    }

    memcpy(Array, &Replica, sizeof(Replica));
}

void Rr_ReserveArray(void *Array, size_t Size, size_t Count, Rr_Arena *Arena)
{
    assert(Array != NULL && "Attempt to grow an array but it's NULL!");
    assert(Arena != NULL && "Attempt to grow an array but Arena is NULL!");

    RR_ARRAY(void) Replica;
    memcpy(&Replica, Array, sizeof(Replica));

    void *Data = NULL;

    Replica.Capacity = Count;
    Data = RR_ALLOC_NO_ZERO(Arena, Size * Count);

    if(Replica.Count)
    {
        memcpy(Data, Replica.Data, Size * Replica.Count);
    }
    Replica.Data = Data;

    memcpy(Array, &Replica, sizeof(Replica));
}

void **Rr_UpsertMap(Rr_Map **Map, Rr_MapKey Key, Rr_Arena *Arena)
{
    if(*Map != NULL)
    {
        for(Rr_MapKey Hash = Key; *Map; Hash <<= 2)
        {
            if(Key == (*Map)->Key)
            {
                return &(*Map)->Value;
            }
            static const int Shift = (sizeof(Rr_MapKey) * CHAR_BIT) - 2;
            Map = &(*Map)->Child[Hash >> Shift];
        }
    }
    if(Arena == NULL)
    {
        return NULL;
    }
    *Map = (Rr_Map *)RR_ALLOC(Arena, sizeof(Rr_Map));
    (*Map)->Key = Key;
    return &(*Map)->Value;
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
    if(OldFirst == NULL)
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

typedef RR_HIVE(void) Rr_VoidHive;
typedef RR_HIVE_GROUP(void) Rr_VoidHiveGroup;
typedef RR_HIVE_ITERATOR(void) Rr_VoidHiveIterator;

void *Rr_AllocHiveGroup(
    size_t ElementSize,
    Rr_HiveSkipType ElementCount,
    Rr_Arena *Arena)
{
    size_t ElementsSize = ElementSize * (size_t)ElementCount;
    size_t SkipsSize = sizeof(Rr_HiveSkipType) * ((size_t)ElementCount + 1);
    size_t TotalAllocSize = sizeof(Rr_VoidHiveGroup) + ElementSize + SkipsSize;
    Rr_VoidHiveGroup *Group = RR_ALLOC(Arena, TotalAllocSize);
    Group->Capacity = ElementCount;
    Group->Elements = RR_ALLOC(Arena, ElementsSize);
    Group->Skips =
        RR_ALLOC_TYPE_COUNT(Arena, Rr_HiveSkipType, ElementCount + 1);
    return Group;
}

void Rr_PushIntoHive(void *Hive, size_t ElementSize, Rr_Arena *Arena)
{
    Rr_VoidHive *VoidHive = Hive;

    if(VoidHive->Last == NULL)
    {
        VoidHive->First =
            Rr_AllocHiveGroup(ElementSize, RR_HIVE_INITIAL, Arena);
        VoidHive->Last = (void *)VoidHive->First;

        VoidHive->Begin.Element = VoidHive->First->Elements;
        VoidHive->Begin.Group = (void *)VoidHive->First;
        VoidHive->Begin.Skip = VoidHive->First->Skips;
    }
    else if(VoidHive->Last->Count == VoidHive->Last->Capacity)
    {
        VoidHive->Last->Next = Rr_AllocHiveGroup(
            ElementSize,
            VoidHive->Last->Capacity * RR_HIVE_GROW,
            Arena);
        VoidHive->Last = (void *)VoidHive->Last->Next;
    }

    Rr_VoidHiveGroup *Group = (Rr_VoidHiveGroup *)VoidHive->Last;

    char *NewElement = (char *)Group->Elements + (Group->Count * ElementSize);
    VoidHive->PushedElement = NewElement;
    Group->Count++;
    VoidHive->Count++;
}

void Rr_BeginHiveIterator(void *Hive, void *It)
{
    Rr_VoidHive *VoidHive = Hive;
    Rr_VoidHiveIterator *VoidIt = It;
    VoidIt->Group = (void *)VoidHive->Begin.Group;
    VoidIt->Skip = VoidHive->Begin.Skip;
    VoidIt->Element = VoidHive->Begin.Element;
}

void Rr_TestHive(void)
{
    typedef struct Rr_Test Rr_Test;
    struct Rr_Test
    {
        uint64_t Test0;
        char Test3;
        uint64_t Test1;
        short Test10;
        double Test2;
        float Test4;
        char Test5;
        float Test6;
    };

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    RR_HIVE(Rr_Test) Hive = { 0 };

    for(char Char = 'a'; Char != 'z' + 1; Char++)
    {
        *RR_PUSH_INTO_HIVE(&Hive, Scratch.Arena) = (Rr_Test){ .Test3 = Char };
        RR_LOG("Pushed: %c", Char);
    }

    RR_FOR_EACH_IN_HIVE(&Hive, It)
    {
        Rr_Test *Element = It.Element;
        RR_LOG("Iterating over: %c", Element->Test3);
    }

    Rr_DestroyScratch(Scratch);
}
