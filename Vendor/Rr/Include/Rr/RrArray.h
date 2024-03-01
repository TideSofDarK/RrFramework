#pragma once

#include <stddef.h>

#include "RrCore.h"

typedef struct SRrArray
{
    void* Data;
    size_t ElementSize;
    size_t Count;
    size_t AllocatedSize;
} SRrArray;

#define RR_ALIGN_OF(Type) offsetof(struct { char W; Type V; }, V)

void RrArray_Reserve(SRrArray* Array, size_t ElementSize, size_t ElementCount, size_t Alignment);

void RrArray_Resize(SRrArray* Array, size_t ElementCount);

void RrArray_Set(SRrArray* Array, size_t Index, void* Data);

void* RrArray_Get(SRrArray* Array, size_t Index);

void RrArray_Emplace(SRrArray* Array, void* Data);

void RrArray_Push(SRrArray* Array, void* Data);

void RrArray_Empty(SRrArray* Array, b32 bFreeAllocation);

#define RrArray_Init(Array, ElementType, ElementCount) RrArray_Reserve(Array, sizeof(ElementType), ElementCount, RR_ALIGN_OF(ElementType))

#ifdef RR_DEBUG
void RrArray_Test(void);
#endif
