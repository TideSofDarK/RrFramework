#ifdef __cplusplus
extern "C" {
#endif

#ifndef RR_HIVE_H
#define RR_HIVE_H

#include <Rr/Rr_Memory.h>

/*
 * Hive (aka Colony)
 *
 * Maintains a list of contiguous element groups.
 * Doesn't reallocate and therefore doesn't invalidate pointers.
 * Uses skiplists to skip erased elements during iteration.
 */

typedef uint16_t Rr_HiveSkipType;

#endif

#define RR_HIVE_CONCAT(a, b)        a##b
#define RR_HIVE_EXPAND_CONCAT(a, b) RR_HIVE_CONCAT(a, b)

#ifndef RR_HIVE_TYPE
#error RR_HIVE_TYPE is not set!
#endif

#ifndef RR_HIVE_TYPE_NAME
#define RR_HIVE_TYPE_NAME RR_HIVE_TYPE
#endif

#ifndef RR_HIVE_NAME
#define RR_HIVE_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE, Hive)
#endif

#ifndef RR_HIVE_GROUP_NAME
#define RR_HIVE_GROUP_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_NAME, Group)
#endif

#ifndef RR_HIVE_ITERATOR_NAME
#define RR_HIVE_ITERATOR_NAME RR_HIVE_EXPAND_CONCAT(RR_HIVE_NAME, Iterator)
#endif

#ifndef RR_HIVE_GROW
#define RR_HIVE_GROW 2
#endif

#ifndef RR_HIVE_INITIAL
#define RR_HIVE_INITIAL 4
#endif

#ifndef RR_HIVE_PREFIX
#define RR_HIVE_PREFIX
#endif

typedef struct RR_HIVE_GROUP_NAME RR_HIVE_GROUP_NAME;
struct RR_HIVE_GROUP_NAME
{
    Rr_HiveSkipType *Skips;
    RR_HIVE_GROUP_NAME *Next;
    RR_HIVE_TYPE *Elements;
    RR_HIVE_GROUP_NAME *Previous;
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
    size_t Count;
    size_t Capacity;
    RR_HIVE_GROUP_NAME *First;
    RR_HIVE_GROUP_NAME *Last;
    RR_HIVE_ITERATOR_NAME Begin;
    RR_HIVE_ITERATOR_NAME End;
};

#define RR_ALLOC_HIVE_GROUP_NAME \
    RR_HIVE_EXPAND_CONCAT(       \
        RR_HIVE_PREFIX,          \
        RR_HIVE_EXPAND_CONCAT(   \
            Alloc,               \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveGroup)))

static inline RR_HIVE_GROUP_NAME *RR_ALLOC_HIVE_GROUP_NAME(
    RR_HIVE_NAME *Hive,
    Rr_HiveSkipType ElementCount,
    Rr_Arena *Arena)
{
    RR_HIVE_GROUP_NAME *Group = RR_ALLOC_TYPE(Arena, RR_HIVE_GROUP_NAME);
    Group->Capacity = ElementCount;
    Group->Elements = RR_ALLOC_TYPE_COUNT(Arena, RR_HIVE_TYPE, ElementCount);
    Group->Skips =
        RR_ALLOC_TYPE_COUNT(Arena, Rr_HiveSkipType, ElementCount + 1);

    if(Hive->Last != NULL)
    {
        Hive->Last->Next = Group;
    }
    Group->Previous = Hive->Last;
    Hive->Last = Group;

    return Group;
}

#define RR_PUSH_INTO_HIVE_NAME \
    RR_HIVE_EXPAND_CONCAT(     \
        RR_HIVE_PREFIX,        \
        RR_HIVE_EXPAND_CONCAT( \
            Push,              \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, IntoHive)))

static inline RR_HIVE_TYPE *RR_PUSH_INTO_HIVE_NAME(
    RR_HIVE_NAME *Hive,
    Rr_Arena *Arena)
{
    if(Hive->First == NULL)
    {
        Hive->First = RR_ALLOC_HIVE_GROUP_NAME(Hive, RR_HIVE_INITIAL, Arena);

        Hive->Begin.Element = Hive->First->Elements;
        Hive->Begin.Group = Hive->First;
        Hive->Begin.Skip = Hive->First->Skips;
    }
    else if(Hive->Last->Count == Hive->Last->Capacity)
    {
        RR_ALLOC_HIVE_GROUP_NAME(
            Hive,
            Hive->Last->Capacity * RR_HIVE_GROW,
            Arena);
    }

    RR_HIVE_GROUP_NAME *Group = Hive->Last;

    RR_HIVE_TYPE *NewElement = Group->Elements + Group->Count;
    Group->Count++;
    Hive->Count++;

    return NewElement;
}

#define RR_GET_HIVE_ITERATOR_NAME \
    RR_HIVE_EXPAND_CONCAT(        \
        RR_HIVE_PREFIX,           \
        RR_HIVE_EXPAND_CONCAT(    \
            Get,                  \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveIterator)))

static inline RR_HIVE_ITERATOR_NAME RR_GET_HIVE_ITERATOR_NAME(
    RR_HIVE_NAME *Hive,
    RR_HIVE_TYPE *Element)
{
    if((Hive)->Last != NULL)
    {
        for(RR_HIVE_GROUP_NAME *Group = Hive->Last; Group != NULL;
            Group = Group->Previous)
        {
            if(Element >= Group->Elements &&
               (uintptr_t)Element < (uintptr_t)Group->Skips)
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

#define RR_HIVE_ITERATOR_VALID_NAME \
    RR_HIVE_EXPAND_CONCAT(          \
        RR_HIVE_PREFIX,             \
        RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveIteratorValid))

static inline bool RR_HIVE_ITERATOR_VALID_NAME(RR_HIVE_ITERATOR_NAME *It)
{
    return It->Group && It->Element && It->Element >= It->Group->Elements &&
           (It->Element < It->Group->Elements + It->Group->Count);
}

#define RR_ADVANCE_HIVE_ITERATOR_NAME \
    RR_HIVE_EXPAND_CONCAT(            \
        RR_HIVE_PREFIX,               \
        RR_HIVE_EXPAND_CONCAT(        \
            Advance,                  \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, HiveIterator)))

static inline void RR_ADVANCE_HIVE_ITERATOR_NAME(RR_HIVE_ITERATOR_NAME *It)
{
    Rr_HiveSkipType Advance = 1 + *It->Skip;
    It->Element += Advance;
    It->Skip += Advance;

    if(It->Element >= (It->Group->Elements + It->Group->Count))
    {
        /* Next group. */

        It->Group = It->Group->Next;
        if(It->Group)
        {
            It->Element = It->Group->Elements;
            It->Skip = It->Group->Skips;
        }
    }
}

#define RR_REMOVE_FROM_HIVE_NAME \
    RR_HIVE_EXPAND_CONCAT(       \
        RR_HIVE_PREFIX,          \
        RR_HIVE_EXPAND_CONCAT(   \
            RemoveFrom,          \
            RR_HIVE_EXPAND_CONCAT(RR_HIVE_TYPE_NAME, Hive)))

static inline void RR_REMOVE_FROM_HIVE_NAME(RR_HIVE_ITERATOR_NAME *It)
{
}

#undef RR_HIVE_TYPE
#undef RR_HIVE_TYPE_NAME
#undef RR_HIVE_NAME
#undef RR_HIVE_GROUP_NAME
#undef RR_HIVE_ITERATOR_NAME
#undef RR_HIVE_GROW
#undef RR_HIVE_INITIAL
#undef RR_HIVE_PREFIX
#undef RR_ALLOC_HIVE_GROUP_NAME
#undef RR_PUSH_INTO_HIVE_NAME
#undef RR_GET_HIVE_ITERATOR_NAME
#undef RR_HIVE_ITERATOR_VALID_NAME
#undef RR_ADVANCE_HIVE_ITERATOR_NAME
#undef RR_REMOVE_FROM_HIVE_NAME

#ifdef __cplusplus
}
#endif
