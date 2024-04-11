#pragma once

#include <stddef.h>

#include "RrCore.h"

typedef void* Rr_Array;

typedef struct Rr_ArrayHeader
{
    size_t ElementSize;
    size_t Count;
    size_t AllocatedSize;
    size_t Alignment;
} Rr_ArrayHeader;

static inline Rr_ArrayHeader* RrArray_Header(Rr_Array Handle)
{
    return (Rr_ArrayHeader*)((char*)Handle - sizeof(Rr_ArrayHeader));
}

Rr_Array Rr_ArrayInit_Internal(size_t ElementSize, size_t ElementCount, size_t Alignment);

Rr_Array Rr_ArrayResize_Internal(Rr_Array Handle, size_t ElementCount);

void Rr_ArrayEmpty_Internal(Rr_Array Handle, b32 bFreeAllocation);

Rr_Array Rr_ArrayPush_Internal(Rr_Array Handle, void* Data);

void Rr_ArraySet(Rr_Array Handle, size_t Index, void* Data);

void* Rr_ArrayGet(Rr_Array Handle, size_t Index);

void Rr_ArrayEmplace(Rr_Array Handle, void* Data);

void Rr_ArrayPop(Rr_Array Handle);

size_t Rr_ArrayCount(Rr_Array Handle);

#define Rr_ArrayInit(Handle, ElementType, ElementCount)                                             \
    {                                                                                               \
        size_t Alignment = 0;                                                                       \
        struct T                                                                                    \
        {                                                                                           \
            char C;                                                                                 \
            ElementType E;                                                                          \
        };                                                                                          \
        (Handle) = Rr_ArrayInit_Internal(sizeof(ElementType), ElementCount, offsetof(struct T, E)); \
    }

#define Rr_ArrayResize(Handle, ElementCount)                      \
    {                                                             \
        (Handle) = Rr_ArrayResize_Internal(Handle, ElementCount); \
    }

#define Rr_ArrayPush(Handle, Element)                        \
    {                                                        \
        (Handle) = Rr_ArrayPush_Internal((Handle), Element); \
    }

#define Rr_ArrayEmpty(Handle)                  \
    {                                          \
        Rr_ArrayEmpty_Internal(Handle, false); \
    }

#define Rr_ArrayFree(Handle)                  \
    {                                         \
        Rr_ArrayEmpty_Internal(Handle, true); \
        (Handle) = NULL;                      \
    }

#ifdef RR_DEBUG
void Rr_Array_Test(void);
#endif
