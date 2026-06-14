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

#ifndef RR_HIVE_H
#define RR_HIVE_H

#include <Rr/Rr_Arena.h>
#include <Rr/Rr_Math.h>

#include <assert.h>
#include <string.h>

/*
 * Hive (aka Colony)
 *
 * Maintains a list of contiguous element groups.
 * Doesn't reallocate and therefore doesn't invalidate pointers.
 * Uses skiplists to skip erased elements during iteration.
 */

typedef uint16_t Rr_HiveSkipType;

#define RR_HIVE_CONCAT(a, b)        a##b
#define RR_HIVE_EXPAND_CONCAT(a, b) RR_HIVE_CONCAT(a, b)

#endif

#ifndef RR_HIVE_PREFIX
#define RR_HIVE_PREFIX
#endif

#ifndef RR_HIVE_TYPE
#error RR_HIVE_TYPE is not set!
#define RR_HIVE_TYPE int
#endif

#ifndef RR_HIVE_TYPE_NAME
#define RR_HIVE_TYPE_NAME RR_HIVE_TYPE
#endif

#ifndef RR_HIVE_NAME
#define RR_HIVE_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE, Hive)
#endif

#ifndef RR_HIVE_GROUP_NAME
#define RR_HIVE_GROUP_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE, HiveGroup)
#endif

#ifndef RR_HIVE_ITERATOR_NAME
#define RR_HIVE_ITERATOR_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE, HiveIterator)
#endif

#ifndef RR_HIVE_MIN_BLOCK_CAPACITY
#define RR_HIVE_MIN_BLOCK_CAPACITY 8
#endif

#ifndef RR_HIVE_MAX_BLOCK_CAPACITY
#define RR_HIVE_MAX_BLOCK_CAPACITY UINT16_MAX
#endif

typedef struct RR_HIVE_GROUP_NAME RR_HIVE_GROUP_NAME;
struct RR_HIVE_GROUP_NAME
{
    Rr_HiveSkipType *Skips;
    RR_HIVE_GROUP_NAME *Next;
    RR_HIVE_TYPE *Elements;
    RR_HIVE_GROUP_NAME *Previous;
    Rr_HiveSkipType FreeListHead;
    Rr_HiveSkipType Capacity;
    Rr_HiveSkipType Count;
    RR_HIVE_GROUP_NAME *NextFree;
    RR_HIVE_GROUP_NAME *PreviousFree;
    size_t GroupNumber;
};

typedef struct RR_HIVE_ITERATOR_NAME RR_HIVE_ITERATOR_NAME;
struct RR_HIVE_ITERATOR_NAME
{
    RR_HIVE_GROUP_NAME *Group;
    Rr_HiveSkipType *Skip;
    RR_HIVE_TYPE *Element;
};

typedef struct RR_HIVE_NAME RR_HIVE_NAME;
struct RR_HIVE_NAME
{
    RR_HIVE_ITERATOR_NAME Begin;
    RR_HIVE_ITERATOR_NAME End;
    RR_HIVE_GROUP_NAME *FreeFirst;
    RR_HIVE_GROUP_NAME *UnusedFirst;
    size_t Count;
    size_t Capacity;
#ifdef RR_DEBUG
    uint16_t TotalGroups;
#endif
};

/* NOTE: Assuming new group will be used immediately. Be
 * careful when implemeting Reserve() and the like! */
#define RR_HIVE_ALLOC_GROUP(Hive_, OutGroup_, ElementCount_, Arena_)    \
    do                                                                  \
    {                                                                   \
        (*OutGroup_) = Rr_Alloc(sizeof(RR_HIVE_GROUP_NAME), (Arena));   \
        (*OutGroup_)->Previous = (Hive_)->End.Group;                    \
        (*OutGroup_)->GroupNumber =                                     \
            ((Hive_)->End.Group) == NULL                                \
                ? 0                                                     \
                : (((Hive_)->End.Group))->GroupNumber + 1u;             \
        (*OutGroup_)->Count = 1;                                        \
        (*OutGroup_)->FreeListHead = UINT16_MAX;                        \
        (*OutGroup_)->Capacity = (ElementCount_);                       \
        (*OutGroup_)->Elements =                                        \
            Rr_Alloc((sizeof(RR_HIVE_TYPE) * ElementCount_), (Arena_)); \
        (*OutGroup_)->Skips = Rr_Alloc(                                 \
            sizeof(Rr_HiveSkipType) * ((ElementCount_) + 1),            \
            (Arena_));                                                  \
    }                                                                   \
    while (0)

#define RR_HIVE_RESET_GROUP(Group, Count_, Next_, Previous_, GroupNumber_) \
    do                                                                     \
    {                                                                      \
        (Group)->Next = Next_;                                             \
        (Group)->Previous = Previous_;                                     \
        (Group)->Count = Count_;                                           \
        (Group)->NextFree = NULL;                                          \
        (Group)->PreviousFree = NULL;                                      \
        (Group)->GroupNumber = GroupNumber_;                               \
        (Group)->FreeListHead = UINT16_MAX;                                \
        memset(                                                            \
            (Group)->Skips,                                                \
            0,                                                             \
            sizeof(Rr_HiveSkipType) * (Group)->Capacity);                  \
    }                                                                      \
    while (0)

#define RR_HIVE_REUSE_GROUP(Hive_, OutGroup_)      \
    do                                             \
    {                                              \
        (*OutGroup_) = (Hive_)->UnusedFirst;       \
        (Hive_)->UnusedFirst = (*OutGroup_)->Next; \
        RR_HIVE_RESET_GROUP(                       \
            (*OutGroup_),                          \
            1,                                     \
            NULL,                                  \
            (Hive_)->End.Group,                    \
            (Hive_)->End.Group->GroupNumber + 1u); \
    }                                              \
    while (0)

#define RR_EDIT_HIVE_FREE_LIST(Ptr_, Offset_, Value_) \
    *(((Rr_HiveSkipType *)(Ptr_)) + (Offset_)) = Value_;

#define RR_EDIT_HIVE_FREE_LIST_HEAD(Ptr_, Value_)         \
    do                                                    \
    {                                                     \
        Rr_HiveSkipType *Ptr = (Rr_HiveSkipType *)(Ptr_); \
        *(((Rr_HiveSkipType *)(Ptr))) = Value_;           \
        *(((Rr_HiveSkipType *)(Ptr)) + 1) = UINT16_MAX;   \
    }                                                     \
    while (0)

#define RR_HIVE_UPDATE_SKIPS(Hive_, It_, PrevFreeListIndex_)              \
    do                                                                    \
    {                                                                     \
        Rr_HiveSkipType NewValue = (Rr_HiveSkipType)(*((It_)->Skip) - 1); \
                                                                          \
        if (NewValue != 0)                                                \
        {                                                                 \
            *((It_)->Skip + NewValue) = *((It_)->Skip + 1) = NewValue;    \
                                                                          \
            ++((Hive_)->FreeFirst->FreeListHead);                         \
                                                                          \
            if ((PrevFreeListIndex_) != UINT16_MAX)                       \
            {                                                             \
                RR_EDIT_HIVE_FREE_LIST(                                   \
                    (It_)->Group->Elements + (PrevFreeListIndex_),        \
                    1,                                                    \
                    (Hive_)->FreeFirst->FreeListHead);                    \
            }                                                             \
                                                                          \
            RR_EDIT_HIVE_FREE_LIST_HEAD(                                  \
                (It_)->Element + 1,                                       \
                (PrevFreeListIndex_));                                    \
        }                                                                 \
        else                                                              \
        {                                                                 \
            (Hive_)->FreeFirst->FreeListHead = (PrevFreeListIndex_);      \
                                                                          \
            if ((PrevFreeListIndex_) != UINT16_MAX)                       \
            {                                                             \
                RR_EDIT_HIVE_FREE_LIST(                                   \
                    (It_)->Group->Elements + (PrevFreeListIndex_),        \
                    1,                                                    \
                    UINT16_MAX);                                          \
            }                                                             \
            else                                                          \
            {                                                             \
                (Hive_)->FreeFirst = (Hive_)->FreeFirst->NextFree;        \
            }                                                             \
        }                                                                 \
                                                                          \
        *((It_)->Skip) = 0;                                               \
        ++((It_)->Group->Count);                                          \
                                                                          \
        if ((It_)->Group == (Hive_)->Begin.Group &&                       \
            (It_)->Element < (Hive_)->Begin.Element)                      \
        {                                                                 \
            (Hive_)->Begin = *(It_);                                      \
        }                                                                 \
                                                                          \
        (Hive_)->Count++;                                                 \
    }                                                                     \
    while (0)

#define RR_HIVE_RESET_GROUP_NUMBER(Hive_)                           \
    do                                                              \
    {                                                               \
        if ((Hive_)->End.Group->GroupNumber == UINT16_MAX)          \
        {                                                           \
            RR_HIVE_GROUP_NAME *UpdateGroup = (Hive_)->Begin.Group; \
            size_t CurrentGroupNumber = 0;                          \
            do                                                      \
            {                                                       \
                UpdateGroup->GroupNumber = CurrentGroupNumber++;    \
                UpdateGroup = UpdateGroup->Next;                    \
            }                                                       \
            while (UpdateGroup != NULL);                            \
        }                                                           \
    }                                                               \
    while (0)

#define RR_HIVE_PUSH_NAME      \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            Push,              \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, IntoHive)))

static inline RR_HIVE_ITERATOR_NAME RR_HIVE_PUSH_NAME(
    RR_HIVE_NAME *Hive,
    Rr_Arena *Arena)
{
    if (Hive->End.Element != NULL)
    {
        if (Hive->FreeFirst == NULL)
        {
            if (Hive->End.Element !=
                (Hive->End.Group->Elements + Hive->End.Group->Capacity))
            {
                RR_HIVE_ITERATOR_NAME ReturnIt = Hive->End;
                Hive->End.Element++;
                Hive->End.Skip++;
                Hive->End.Group->Count++;
                Hive->Count++;
                return ReturnIt;
            }

            RR_HIVE_GROUP_NAME *NextGroup;
            if (Hive->UnusedFirst == NULL)
            {
                Rr_HiveSkipType NewCapacity = (Rr_HiveSkipType)(RR_MIN(
                    Hive->Count,
                    (size_t)(RR_HIVE_MAX_BLOCK_CAPACITY)));
                RR_HIVE_RESET_GROUP_NUMBER(Hive);

                RR_HIVE_ALLOC_GROUP(Hive, &NextGroup, NewCapacity, Arena);

#ifdef RR_DEBUG
                Hive->TotalGroups++;
#endif
                Hive->Capacity += NewCapacity;
            }
            else
            {
                RR_HIVE_REUSE_GROUP(Hive, &NextGroup);
            }

            Hive->End.Group->Next = NextGroup;
            Hive->End.Group = NextGroup;
            Hive->End.Element = NextGroup->Elements + 1;
            Hive->End.Skip = NextGroup->Skips + 1;
            Hive->Count++;

            RR_HIVE_ITERATOR_NAME ReturnIt;
            ReturnIt.Group = NextGroup;
            ReturnIt.Element = NextGroup->Elements;
            ReturnIt.Skip = NextGroup->Skips;

            return ReturnIt;
        }
        else
        {
            RR_HIVE_ITERATOR_NAME ReturnIt;
            ReturnIt.Group = Hive->FreeFirst;
            ReturnIt.Element =
                ReturnIt.Group->Elements + ReturnIt.Group->FreeListHead;
            ReturnIt.Skip =
                ReturnIt.Group->Skips + ReturnIt.Group->FreeListHead;

            Rr_HiveSkipType PrevFreeListIndex =
                *(Rr_HiveSkipType *)ReturnIt.Element;
            RR_HIVE_UPDATE_SKIPS(Hive, &ReturnIt, PrevFreeListIndex);

            return ReturnIt;
        }
    }
    else
    {
        RR_HIVE_ALLOC_GROUP(
            Hive,
            &Hive->Begin.Group,
            RR_HIVE_MIN_BLOCK_CAPACITY,
            Arena);

        Hive->End.Group = Hive->Begin.Group;

#ifdef RR_DEBUG
        Hive->TotalGroups++;
#endif

        Hive->End.Element = Hive->Begin.Element = Hive->Begin.Group->Elements;
        Hive->End.Skip = Hive->Begin.Skip = Hive->Begin.Group->Skips;

        Hive->End.Element++;
        Hive->End.Skip++;

        Hive->Capacity = RR_HIVE_MIN_BLOCK_CAPACITY;
        Hive->Count = 1;

        return Hive->Begin;
    }
}

#define RR_HIVE_GET_ITERATOR_NAME \
    RR_HIVE_EXPAND_CONCAT(        \
        RR_HIVE_PREFIX,           \
        RR_HIVE_EXPAND_CONCAT(    \
            Get,                  \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveIterator)))

static inline RR_HIVE_ITERATOR_NAME RR_HIVE_GET_ITERATOR_NAME(
    RR_HIVE_NAME *Hive,
    RR_HIVE_TYPE *Element)
{
    if (Hive->End.Group != NULL)
    {
        /* TODO: Align element pointer! */

        if (Element >= Hive->End.Group->Elements &&
            Element < (Hive->End.Group->Elements + Hive->End.Group->Capacity))
        {
            RR_HIVE_ITERATOR_NAME It;
            It.Group = Hive->End.Group;
            It.Skip =
                Hive->End.Group->Skips + (Element - Hive->End.Group->Elements);
            It.Element = Element;

            return It;
        }

        for (RR_HIVE_GROUP_NAME *Group = Hive->End.Group; Group != NULL;
             Group = Group->Previous)
        {
            if (Element >= Group->Elements &&
                Element < (Group->Elements + Group->Capacity))
            {
                RR_HIVE_ITERATOR_NAME It;
                It.Group = Group;
                It.Skip = Group->Skips + (Element - Group->Elements);
                It.Element = Element;

                return It;
            }
        }
    }

    return Hive->End;
}

#define RR_HIVE_BEGIN_NAME     \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            BeginIn,           \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, Hive)))

static inline RR_HIVE_ITERATOR_NAME RR_HIVE_BEGIN_NAME(RR_HIVE_NAME *Hive)
{
    return Hive->Begin;
}

#define RR_HIVE_NEXT_NAME      \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            NextIn,            \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, Hive)))

static inline void RR_HIVE_NEXT_NAME(RR_HIVE_ITERATOR_NAME *It)
{
    assert(It->Group != NULL);
    Rr_HiveSkipType Skip = *(++It->Skip);

    if ((It->Element += (size_t)(Skip) + 1U) ==
            (It->Group->Elements + It->Group->Capacity) &&
        It->Group->Next != NULL)
    {
        It->Group = It->Group->Next;
        RR_HIVE_TYPE *Elements = It->Group->Elements;
        Rr_HiveSkipType *Skips = It->Group->Skips;
        Skip = *Skips;
        It->Element = Elements + Skip;
        It->Skip = Skips;
    }

    It->Skip += Skip;
}

#define RR_HIVE_IS_END_NAME    \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            Is,                \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveEnd)))

static inline bool RR_HIVE_IS_END_NAME(
    RR_HIVE_NAME *Hive,
    RR_HIVE_ITERATOR_NAME *It)
{
    return It->Element == Hive->End.Element;
}

#define RR_REMOVE_FROM_ERASURE_GROUPS(Hive_, Group_)                       \
    do                                                                     \
    {                                                                      \
        if ((Group_) != (Hive_)->FreeFirst)                                \
        {                                                                  \
            (Group_)->PreviousFree->NextFree = (Group_)->NextFree;         \
                                                                           \
            if ((Group_)->NextFree != NULL)                                \
            {                                                              \
                (Group_)->NextFree->PreviousFree = (Group_)->PreviousFree; \
            }                                                              \
        }                                                                  \
        else                                                               \
        {                                                                  \
            (Hive_)->FreeFirst = (Hive_)->FreeFirst->NextFree;             \
        }                                                                  \
    }                                                                      \
    while (0)

#define RR_MOVE_HIVE_GROUP_TO_UNUSED_LIST(Hive_, Group_) \
    do                                                   \
    {                                                    \
        (Group_)->Next = (Hive_)->UnusedFirst;           \
        (Hive_)->UnusedFirst = (Group_);                 \
    }                                                    \
    while (0)

#define RR_HIVE_RESET_ONLY_GROUP(Hive_, Group_)                          \
    do                                                                   \
    {                                                                    \
        (Hive_)->FreeFirst = NULL;                                       \
        RR_HIVE_RESET_GROUP((Group_), 0, NULL, NULL, 0);                 \
        (Hive_)->End.Element = Hive->Begin.Element =                     \
            Hive->Begin.Group->Elements;                                 \
        (Hive_)->End.Skip = Hive->Begin.Skip = Hive->Begin.Group->Skips; \
    }                                                                    \
    while (0)

#define RR_HIVE_ERASE_NAME     \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            EraseFrom,         \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, Hive)))

static inline void RR_HIVE_ERASE_NAME(
    RR_HIVE_NAME *Hive,
    RR_HIVE_ITERATOR_NAME *It)
{
    assert(Hive->Count != 0);
    assert(It->Group != NULL);
    assert(It->Element != Hive->End.Element);
    assert(*(It->Skip) == 0);

    Hive->Count--;

    if (--(It->Group->Count) != 0) /* Not the last element in the group. */
    {
        char PrevSkip = *(It->Skip - (It->Skip != It->Group->Skips)) != 0;
        char AfterSkip = *(It->Skip + 1) != 0;
        Rr_HiveSkipType UpdateValue = 1;

        if (!(PrevSkip | AfterSkip))
        {
            *It->Skip = 1;
            Rr_HiveSkipType Index =
                (Rr_HiveSkipType)(It->Element - It->Group->Elements);
            if (It->Group->FreeListHead != UINT16_MAX)
            {
                RR_EDIT_HIVE_FREE_LIST(
                    It->Group->Elements + It->Group->FreeListHead,
                    1,
                    Index);
            }
            else
            {
                It->Group->NextFree = Hive->FreeFirst;

                if (Hive->FreeFirst != NULL)
                {
                    Hive->FreeFirst->PreviousFree = It->Group;
                }

                Hive->FreeFirst = It->Group;
            }

            RR_EDIT_HIVE_FREE_LIST_HEAD(It->Element, It->Group->FreeListHead);
            It->Group->FreeListHead = Index;
        }
        else if (PrevSkip && (!AfterSkip))
        {
            *(It->Skip - *(It->Skip - 1)) = *It->Skip =
                (Rr_HiveSkipType)(*(It->Skip - 1) + 1);
        }
        else if ((!PrevSkip) && AfterSkip)
        {
            Rr_HiveSkipType FollowingValue =
                (Rr_HiveSkipType)(*(It->Skip + 1) + 1);
            *(It->Skip + FollowingValue - 1) = *(It->Skip) = FollowingValue;

            Rr_HiveSkipType FollowingPrevious =
                *((Rr_HiveSkipType *)(It->Element + 1));
            Rr_HiveSkipType FollowingNext =
                *((Rr_HiveSkipType *)(It->Element + 1) + 1);
            RR_EDIT_HIVE_FREE_LIST(It->Element, 0, FollowingPrevious);
            RR_EDIT_HIVE_FREE_LIST(It->Element, 1, FollowingNext);

            Rr_HiveSkipType Index =
                (Rr_HiveSkipType)(It->Element - It->Group->Elements);

            if (FollowingPrevious != UINT16_MAX)
            {
                RR_EDIT_HIVE_FREE_LIST(
                    It->Group->Elements + FollowingPrevious,
                    1,
                    Index);
            }

            if (FollowingNext != UINT16_MAX)
            {
                RR_EDIT_HIVE_FREE_LIST(
                    It->Group->Elements + FollowingNext,
                    0,
                    Index);
            }
            else
            {
                It->Group->FreeListHead = Index;
            }

            UpdateValue = FollowingValue;
        }
        else
        {
            *It->Skip = 1;
            Rr_HiveSkipType PrecedingValue = *(It->Skip - 1);
            Rr_HiveSkipType FollowingValue =
                (Rr_HiveSkipType)(*(It->Skip + 1) + 1);

            *(It->Skip - PrecedingValue) = *(It->Skip + FollowingValue - 1) =
                (Rr_HiveSkipType)(PrecedingValue + FollowingValue);

            Rr_HiveSkipType FollowingPrevious =
                *((Rr_HiveSkipType *)(It->Element + 1));
            Rr_HiveSkipType FollowingNext =
                *((Rr_HiveSkipType *)(It->Element + 1) + 1);

            if (FollowingPrevious != UINT16_MAX)
            {
                RR_EDIT_HIVE_FREE_LIST(
                    It->Group->Elements + FollowingPrevious,
                    1,
                    FollowingNext);
            }

            if (FollowingNext != UINT16_MAX)
            {
                RR_EDIT_HIVE_FREE_LIST(
                    It->Group->Elements + FollowingNext,
                    0,
                    FollowingPrevious);
            }
            else
            {
                It->Group->FreeListHead = FollowingPrevious;
            }

            UpdateValue = FollowingValue;
        }

        RR_HIVE_ITERATOR_NAME ReturnIt;
        ReturnIt.Group = It->Group;
        ReturnIt.Element = It->Element + UpdateValue;
        ReturnIt.Skip = It->Skip + UpdateValue;

        if (ReturnIt.Element == (It->Group->Elements + It->Group->Capacity) &&
            It->Group != Hive->End.Group)
        {
            ReturnIt.Group = It->Group->Next;
            RR_HIVE_TYPE *Elements = ReturnIt.Group->Elements;
            Rr_HiveSkipType *Skips = ReturnIt.Group->Skips;
            ReturnIt.Element = Elements + *Skips;
            ReturnIt.Skip = Skips + *Skips;
        }

        if (It->Element == Hive->Begin.Element)
        {
            Hive->Begin = ReturnIt;
        }

        *It = ReturnIt;
        return;
    }

    bool InBackBlock = It->Group->Next == NULL;
    bool InFrontBlock = It->Group == Hive->Begin.Group;

    if (InBackBlock && InFrontBlock) /* Only block in hive. */
    {
        RR_HIVE_RESET_ONLY_GROUP(Hive, It->Group);

        *It = Hive->End;
    }
    else if ((!InBackBlock) && InFrontBlock) /* Add the first group to unused
                                               list. */
    {
        It->Group->Next->Previous = NULL;
        Hive->Begin.Group = It->Group->Next; /* We can assume Next is valid. */

        if (It->Group->FreeListHead != UINT16_MAX)
        {
            RR_REMOVE_FROM_ERASURE_GROUPS(Hive, It->Group);
        }

        RR_MOVE_HIVE_GROUP_TO_UNUSED_LIST(Hive, It->Group);

        Hive->Begin.Element =
            Hive->Begin.Group->Elements + *Hive->Begin.Group->Skips;
        Hive->Begin.Skip = Hive->Begin.Group->Skips + *Hive->Begin.Group->Skips;

        *It = Hive->Begin;
    }
    else if (!(InBackBlock ||
               InFrontBlock)) /* Neither first, nor last group. */
    {
        It->Group->Next->Previous = It->Group->Previous;
        RR_HIVE_GROUP_NAME *ReturnGroup = It->Group->Previous->Next =
            It->Group->Next;

        if (It->Group->FreeListHead != UINT16_MAX)
        {
            RR_REMOVE_FROM_ERASURE_GROUPS(Hive, It->Group);
        }

        RR_MOVE_HIVE_GROUP_TO_UNUSED_LIST(Hive, It->Group);

        RR_HIVE_ITERATOR_NAME ReturnIt;

        ReturnIt.Group = ReturnGroup;
        ReturnIt.Element = ReturnGroup->Elements + *(ReturnGroup->Skips);
        ReturnIt.Skip = ReturnGroup->Skips + *(ReturnGroup->Skips);

        *It = ReturnIt;
    }
    else /* It's the final group. */
    {
        if (It->Group->FreeListHead != UINT16_MAX)
        {
            RR_REMOVE_FROM_ERASURE_GROUPS(Hive, It->Group);
        }

        It->Group->Previous->Next = NULL;

        Hive->End.Group = It->Group->Previous;
        Hive->End.Element =
            Hive->End.Group->Elements + Hive->End.Group->Capacity;
        Hive->End.Skip = Hive->End.Group->Skips + Hive->End.Group->Capacity;

        RR_MOVE_HIVE_GROUP_TO_UNUSED_LIST(Hive, It->Group);

        *It = Hive->End;
    }
}

#define RR_HIVE_CLEAR_NAME     \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            Clear,             \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, Hive)))

static inline void RR_HIVE_CLEAR_NAME(RR_HIVE_NAME *Hive)
{
    if (Hive->Count == 0)
    {
        return;
    }

    if (Hive->Begin.Group != Hive->End.Group)
    {
        /* Move the rest of the groups to unused list. */

        Hive->End.Group->Next = Hive->UnusedFirst;
        Hive->UnusedFirst = Hive->Begin.Group->Next;
        Hive->End.Group = Hive->Begin.Group;
    }

    RR_HIVE_RESET_ONLY_GROUP(Hive, Hive->Begin.Group);
    Hive->Count = 0;
}

#undef RR_HIVE_PREFIX
#undef RR_HIVE_TYPE
#undef RR_HIVE_TYPE_NAME
#undef RR_HIVE_NAME
#undef RR_HIVE_GROUP_NAME
#undef RR_HIVE_ITERATOR_NAME
#undef RR_HIVE_MIN_BLOCK_CAPACITY
#undef RR_HIVE_MAX_BLOCK_CAPACITY
#undef RR_HIVE_ALLOC_GROUP
#undef RR_HIVE_REUSE_GROUP
#undef RR_HIVE_UPDATE_SKIPS
#undef RR_HIVE_RESET_GROUP
#undef RR_HIVE_RESET_ONLY_GROUP
#undef RR_HIVE_PUSH_NAME
#undef RR_HIVE_GET_ITERATOR_NAME
#undef RR_HIVE_BEGIN_NAME
#undef RR_HIVE_NEXT_NAME
#undef RR_HIVE_IS_END_NAME
#undef RR_HIVE_ERASE_NAME
#undef RR_HIVE_CLEAR_NAME
