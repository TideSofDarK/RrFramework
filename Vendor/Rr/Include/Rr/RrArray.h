#pragma once

#include <stddef.h>

#include "RrCore.h"

typedef void* SRrArray;

typedef struct SRrArrayHeader
{
    size_t ElementSize;
    size_t Count;
    size_t AllocatedSize;
    size_t Alignment;
} SRrArrayHeader;

static inline SRrArrayHeader* RrArray_Header(SRrArray Handle)
{
    return (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));
}

SRrArray RrArray_Init_Internal(size_t ElementSize, size_t ElementCount, size_t Alignment);

SRrArray RrArray_Resize_Internal(SRrArray Handle, size_t ElementCount);

void RrArray_Empty_Internal(SRrArray Handle, b32 bFreeAllocation);

SRrArray RrArray_Push_Internal(SRrArray Handle, void* Data);

void RrArray_Set(SRrArray Handle, size_t Index, void* Data);

void* RrArray_Get(SRrArray Handle, size_t Index);

void RrArray_Emplace(SRrArray Handle, void* Data);

void RrArray_Pop(SRrArray Handle);

size_t RrArray_Count(SRrArray Handle);

#define RrArray_Init(Handle, ElementType, ElementCount)                                             \
    {                                                                                               \
        size_t Alignment = 0;                                                                       \
        struct T                                                                                    \
        {                                                                                           \
            char C;                                                                                 \
            ElementType E;                                                                          \
        };                                                                                          \
        (Handle) = RrArray_Init_Internal(sizeof(ElementType), ElementCount, offsetof(struct T, E)); \
    }

#define RrArray_Resize(Handle, ElementCount)                      \
    {                                                             \
        (Handle) = RrArray_Resize_Internal(Handle, ElementCount); \
    }

#define RrArray_Push(Handle, Element)                        \
    {                                                        \
        (Handle) = RrArray_Push_Internal((Handle), Element); \
    }

#define RrArray_Empty(Handle, bFreeAllocation)           \
    {                                                    \
        RrArray_Empty_Internal(Handle, bFreeAllocation); \
        (Handle) = (bFreeAllocation) ? NULL : (Handle);  \
    }

#ifdef RR_DEBUG
void RrArray_Test(void);
#endif
