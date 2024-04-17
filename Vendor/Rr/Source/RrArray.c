#include "RrArray.h"

#include <stdlib.h>

#include <SDL3/SDL_log.h>
#include <SDL3/SDL_assert.h>

#include "RrMemory.h"

Rr_Array Rr_ArrayInit_Internal(size_t ElementSize, size_t ElementCount, size_t Alignment)
{
    size_t request_size = ElementCount * ElementSize + Alignment + sizeof(Rr_ArrayHeader);
    char* Data = (char*)(Rr_Calloc(1, request_size));

    size_t Remainder = ((size_t)Data + sizeof(Rr_ArrayHeader)) % Alignment;
    size_t Offset = Alignment - Remainder;
    char* NewHandle = Data + (unsigned char)Offset + sizeof(Rr_ArrayHeader);

    *RrArray_Header(NewHandle) = (Rr_ArrayHeader){
        .Alignment = Alignment,
        .AllocatedSize = ElementCount * ElementSize,
        .ElementSize = ElementSize,
        .Count = 0
    };

    *(unsigned char*)(NewHandle - sizeof(Rr_ArrayHeader) - 1) = Offset;

    return NewHandle;
}

Rr_Array Rr_ArrayResize_Internal(Rr_Array Handle, size_t ElementCount)
{
    Rr_Array OldHandle = Handle;
    Rr_ArrayHeader* OldHeader = RrArray_Header(OldHandle);
    size_t NewAllocatedSize = OldHeader->ElementSize * ElementCount;
    Handle = Rr_ArrayInit_Internal(OldHeader->ElementSize, ElementCount, OldHeader->Alignment);
    Rr_ArrayHeader* NewHeader = RrArray_Header(Handle);
    NewHeader->Count = SDL_min(ElementCount, OldHeader->Count);
    SDL_memcpy(Handle, OldHandle, SDL_min(OldHeader->AllocatedSize, NewAllocatedSize));
    Rr_ArrayFree(OldHandle);
    return Handle;
}

void Rr_ArrayEmpty_Internal(Rr_Array Handle, b32 bFreeAllocation)
{
    if (bFreeAllocation)
    {
        int Offset = *(((u8*)Handle) - 1 - sizeof(Rr_ArrayHeader));
        Rr_Free((u8*)Handle - sizeof(Rr_ArrayHeader) - Offset);
        return;
    }
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    Header->Count = 0;
}

Rr_Array Rr_ArrayPush_Internal(Rr_Array Handle, void* Data)
{
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    if (Header->Count * Header->ElementSize >= Header->AllocatedSize)
    {
        Handle = Rr_ArrayResize_Internal(Handle, Header->Count * 2);
    }
    Header = RrArray_Header(Handle);

    Rr_ArraySet(Handle, Header->Count++, Data);

    return Handle;
}

void Rr_ArraySet(Rr_Array Handle, size_t Index, void* Data)
{
    if (Handle == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Attempting to set an element but the Handle is NULL!");
        abort();
    }
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    if (Index * Header->ElementSize >= Header->AllocatedSize)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Index %zu is out-of-bounds! AllocationSize is %zu, ElementSize is %zu.", Index, Header->AllocatedSize, Header->ElementSize);
        abort();
    }
    else
    {
        SDL_memcpy((u8*)Handle + (Index * Header->ElementSize), Data, Header->ElementSize);
    }
}

void* Rr_ArrayGet(Rr_Array Handle, size_t Index)
{
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    return (u8*)Handle + (Index * Header->ElementSize);
}

void Rr_ArrayEmplace(Rr_Array Handle, void* Data)
{
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    Rr_ArraySet(Handle, Header->Count++, Data);
}

void Rr_ArrayPop(Rr_Array Handle)
{
    Rr_ArrayHeader* Header = RrArray_Header(Handle);
    if (Header->Count > 0)
    {
        Header->Count--;
    }
}

size_t Rr_ArrayCount(Rr_Array Handle)
{
    return RrArray_Header(Handle)->Count;
}

#ifdef RR_DEBUG

typedef struct SArrayEntry
{
    float Color[4];
    float Position[3];
} SArrayItem;

void Rr_Array_Test(void)
{
    size_t InitialAllocations = SDL_GetNumAllocations();
    Rr_Array Handle = { 0 };
    Rr_ArrayInit(Handle, SArrayItem, 4);
    Rr_ArrayHeader* Header = (Rr_ArrayHeader*)((char*)Handle - sizeof(Rr_ArrayHeader));

    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 4);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    Rr_ArrayResize(Handle, 8);
    Header = RrArray_Header(Handle);
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 8);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    Rr_ArrayResize(Handle, 1);
    Header = RrArray_Header(Handle);
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 1);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    Rr_ArrayEmplace(Handle, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    SDL_assert(Header->Count == 1);
    SArrayItem* FirstItem = Rr_ArrayGet(Handle, 0);
    SDL_assert(FirstItem->Position[0] == 1.0f);
    SDL_assert(FirstItem->Color[2] == 1.0f);

    Rr_ArrayPush(Handle, &((SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } }));
    Header = RrArray_Header(Handle);
    SDL_assert(Header->Count == 2);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 2);
    Rr_ArrayPush(Handle, &((SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } }));
    Header = RrArray_Header(Handle);
    SDL_assert(Header->Count == 3);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 4);
    SArrayItem* ThirdItem = Rr_ArrayGet(Handle, 2);
    SDL_assert(ThirdItem->Position[0] == 1.0f);
    SDL_assert(ThirdItem->Color[2] == 1.0f);

    size_t CurrentAllocationSize = Header->AllocatedSize;
    Rr_ArrayEmpty(Handle);
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == CurrentAllocationSize);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    Rr_ArrayFree(Handle);
    SDL_assert(InitialAllocations == SDL_GetNumAllocations());
}
#endif
