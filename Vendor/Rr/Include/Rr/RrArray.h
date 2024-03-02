#pragma once

#include <stddef.h>

#include "RrCore.h"

typedef void* SRrArray;

void RrArray_Reserve(SRrArray* Handle, size_t ElementSize, size_t ElementCount, size_t Alignment);

void RrArray_Resize(SRrArray* Handle, size_t ElementCount);

void RrArray_Set(SRrArray Handle, size_t Index, void* Data);

void* RrArray_Get(SRrArray Handle, size_t Index);

void RrArray_Emplace(SRrArray Handle, void* Data);

void RrArray_Push(SRrArray* Handle, void* Data);

void RrArray_Pop(SRrArray Handle);

void RrArray_Empty(SRrArray Handle, b32 bFreeAllocation);

size_t RrArray_Count(SRrArray Handle);

#define RrArray_Init(Handle, ElementType, ElementCount)                                             \
    {                                                                                               \
        size_t Alignment = 0;                                                                       \
        struct T                                                                                    \
        {                                                                                           \
            char C;                                                                                 \
            ElementType E;                                                                          \
        };                                                                                          \
        RrArray_Reserve((void*)(Handle), sizeof(ElementType), ElementCount, offsetof(struct T, E)); \
    }

#ifdef RR_DEBUG
void RrArray_Test(void);
#endif
