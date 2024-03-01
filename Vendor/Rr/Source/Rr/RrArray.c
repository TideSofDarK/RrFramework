#include "RrArray.h"

#include <SDL3/SDL.h>

void RrArray_Reserve(SRrArray* Array, size_t ElementSize, size_t ElementCount, size_t Alignment)
{
    if (Array->Data != NULL)
    {
        Array->ElementSize = ElementSize;
        if (Array->AllocatedSize < ElementCount * ElementSize)
        {
            RrArray_Resize(Array, ElementCount);
        }
    }
    else
    {
        Array->AllocatedSize = ElementCount * ElementSize;
        Array->Data = SDL_aligned_alloc(Alignment, Array->AllocatedSize);
        Array->ElementSize = ElementSize;
        Array->Count = 0;
    }
}

void RrArray_Resize(SRrArray* Array, size_t ElementCount)
{
    void* OldData = Array->Data;
    size_t NewAllocatedSize = Array->ElementSize * ElementCount;
    Array->Data = SDL_aligned_alloc(16, NewAllocatedSize);
    if (OldData != NULL)
    {
        SDL_memcpy(Array->Data, OldData, SDL_min(Array->AllocatedSize, NewAllocatedSize));
        SDL_aligned_free(OldData);
    }
    Array->AllocatedSize = Array->ElementSize * ElementCount;
    Array->Count = SDL_min(ElementCount, Array->Count);
}

void RrArray_Set(SRrArray* Array, size_t Index, void* Data)
{
    if (Array->Data == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Attempting to set an element but the Data is NULL!");
        SDL_assert(0);
    }
    if (Index * Array->ElementSize >= Array->AllocatedSize)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Index %zu is out-of-bounds! AllocationSize is %zu, ElementSize is %zu.", Index, Array->AllocatedSize, Array->ElementSize);
        SDL_assert(0);
    }
    SDL_memcpy((u8*)Array->Data + (Index * Array->ElementSize), Data, Array->ElementSize);
}

void* RrArray_Get(SRrArray* Array, size_t Index)
{
    return (u8*)Array->Data + (Index * Array->ElementSize);
}

void RrArray_Emplace(SRrArray* Array, void* Data)
{
    RrArray_Set(Array, Array->Count++, Data);
}

void RrArray_Push(SRrArray* Array, void* Data)
{
    if (Array->Count * Array->ElementSize >= Array->AllocatedSize)
    {
        RrArray_Resize(Array, Array->Count * 2);
    }

    RrArray_Set(Array, Array->Count++, Data);
}

void RrArray_Empty(SRrArray* Array, b32 bFreeAllocation)
{
    if (bFreeAllocation)
    {
        SDL_aligned_free(Array->Data);
        Array->Data = NULL;
        Array->AllocatedSize = 0;
    }
    Array->Count = 0;
}

#ifdef RR_DEBUG

typedef struct SArrayEntry
{
    float Color[4];
    float Position[3];
} SArrayItem;

void RrArray_Test(void)
{
    SRrArray TestArray = { 0 };
    RrArray_Init(&TestArray, SArrayItem, 4);

    SDL_assert(TestArray.Count == 0);
    SDL_assert(TestArray.AllocatedSize == sizeof(SArrayItem) * 4);
    SDL_assert(TestArray.ElementSize == sizeof(SArrayItem));
    SDL_assert(TestArray.Data != NULL);

    RrArray_Resize(&TestArray, 8);
    SDL_assert(TestArray.Count == 0);
    SDL_assert(TestArray.AllocatedSize == sizeof(SArrayItem) * 8);
    SDL_assert(TestArray.ElementSize == sizeof(SArrayItem));
    SDL_assert(TestArray.Data != NULL);

    RrArray_Resize(&TestArray, 1);
    SDL_assert(TestArray.Count == 0);
    SDL_assert(TestArray.AllocatedSize == sizeof(SArrayItem) * 1);
    SDL_assert(TestArray.ElementSize == sizeof(SArrayItem));
    SDL_assert(TestArray.Data != NULL);

    RrArray_Emplace(&TestArray, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    SDL_assert(TestArray.Count == 1);
    SArrayItem* FirstItem = RrArray_Get(&TestArray, 0);
    SDL_assert(FirstItem->Position[0] == 1.0f);
    SDL_assert(FirstItem->Color[2] == 1.0f);

    RrArray_Push(&TestArray, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    SDL_assert(TestArray.Count == 2);
    SDL_assert(TestArray.AllocatedSize == sizeof(SArrayItem) * 2);
    RrArray_Push(&TestArray, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    SDL_assert(TestArray.Count == 3);
    SDL_assert(TestArray.AllocatedSize == sizeof(SArrayItem) * 4);
    SArrayItem* ThirdItem = RrArray_Get(&TestArray, 2);
    SDL_assert(ThirdItem->Position[0] == 1.0f);
    SDL_assert(ThirdItem->Color[2] == 1.0f);

    size_t CurrentAllocationSize = TestArray.AllocatedSize;
    RrArray_Empty(&TestArray, FALSE);
    SDL_assert(TestArray.Count == 0);
    SDL_assert(TestArray.AllocatedSize == CurrentAllocationSize);
    SDL_assert(TestArray.ElementSize == sizeof(SArrayItem));
    SDL_assert(TestArray.Data != NULL);

    RrArray_Empty(&TestArray, TRUE);
    SDL_assert(TestArray.Count == 0);
    SDL_assert(TestArray.AllocatedSize == 0);
    SDL_assert(TestArray.ElementSize == sizeof(SArrayItem));
    SDL_assert(TestArray.Data == NULL);
}
#endif
