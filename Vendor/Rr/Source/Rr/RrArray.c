#include "RrArray.h"

#include <SDL3/SDL.h>

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

void RrArray_Reserve(SRrArray* Handle, size_t ElementSize, size_t ElementCount, size_t Alignment)
{
    size_t request_size = ElementCount * ElementSize + Alignment + sizeof(SRrArrayHeader);
    char* Data = (char*)(SDL_calloc(1, request_size));

    size_t Remainder = ((size_t)Data + sizeof(SRrArrayHeader)) % Alignment;
    size_t Offset = Alignment - Remainder;
    char* NewHandle = Data + (unsigned char)Offset + sizeof(SRrArrayHeader);

    *RrArray_Header(NewHandle) = (SRrArrayHeader){
        .Alignment = Alignment,
        .AllocatedSize = ElementCount * ElementSize,
        .ElementSize = ElementSize,
        .Count = 0
    };

    *(unsigned char*)(NewHandle - sizeof(SRrArrayHeader) - 1) = Offset;

    *Handle = (SRrArray)NewHandle;
}

void RrArray_Resize(SRrArray* Handle, size_t ElementCount)
{
    SRrArray OldHandle = *Handle;
    SRrArrayHeader* OldHeader = RrArray_Header(OldHandle);
    size_t NewAllocatedSize = OldHeader->ElementSize * ElementCount;
    *Handle = NULL;
    RrArray_Reserve(Handle, OldHeader->ElementSize, ElementCount, OldHeader->Alignment);
    SRrArrayHeader* NewHeader = RrArray_Header(*Handle);
    NewHeader->Count = SDL_min(ElementCount, OldHeader->Count);
    SDL_memcpy(*Handle, OldHandle, SDL_min(OldHeader->AllocatedSize, NewAllocatedSize));
    RrArray_Empty(OldHandle, TRUE);
}

void RrArray_Set(SRrArray Handle, size_t Index, void* Data)
{
    if (Handle == NULL)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Attempting to set an element but the Handle is NULL!");
        exit(0);
    }
    SRrArrayHeader* Header = RrArray_Header(Handle);
    if (Index * Header->ElementSize >= Header->AllocatedSize)
    {
        SDL_LogError(SDL_LOG_CATEGORY_SYSTEM, "RrArray: Index %zu is out-of-bounds! AllocationSize is %zu, ElementSize is %zu.", Index, Header->AllocatedSize, Header->ElementSize);
        exit(0);
    }
    else
    {
        SDL_memcpy((u8*)Handle + (Index * Header->ElementSize), Data, Header->ElementSize);
    }
}

void* RrArray_Get(SRrArray Handle, size_t Index)
{
    SRrArrayHeader* Header = RrArray_Header(Handle);
    return (u8*)Handle + (Index * Header->ElementSize);
}

void RrArray_Emplace(SRrArray Handle, void* Data)
{
    SRrArrayHeader* Header = RrArray_Header(Handle);
    RrArray_Set(Handle, Header->Count++, Data);
}

void RrArray_Push(SRrArray* Handle, void* Data)
{
    SRrArrayHeader* Header = RrArray_Header(*Handle);
    if (Header->Count * Header->ElementSize >= Header->AllocatedSize)
    {
        RrArray_Resize(Handle, Header->Count * 2);
    }
    Header = RrArray_Header(*Handle);

    RrArray_Set(*Handle, Header->Count++, Data);
}

void RrArray_Empty(SRrArray Handle, b32 bFreeAllocation)
{
    if (bFreeAllocation)
    {
        int Offset = *(((u8*)Handle) - 1 - sizeof(SRrArrayHeader));
        SDL_free((u8*)Handle - sizeof(SRrArrayHeader) - Offset);
        return;
    }
    SRrArrayHeader* Header = RrArray_Header(Handle);
    Header->Count = 0;
}

size_t RrArray_Count(SRrArray Handle)
{
    SRrArrayHeader* Header = RrArray_Header(Handle);
    return Header->Count;
}

#ifdef RR_DEBUG

typedef struct SArrayEntry
{
    float Color[4];
    float Position[3];
} SArrayItem;

void RrArray_Test(void)
{
    size_t InitialAllocations = SDL_GetNumAllocations();
    SRrArray Handle = { 0 };
    RrArray_Init(&Handle, SArrayItem, 4);
    SRrArrayHeader* Header = (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));

    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 4);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    RrArray_Resize(&Handle, 8);
    Header = (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 8);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    RrArray_Resize(&Handle, 1);
    Header = (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 1);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    RrArray_Emplace(Handle, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    SDL_assert(Header->Count == 1);
    SArrayItem* FirstItem = RrArray_Get(Handle, 0);
    SDL_assert(FirstItem->Position[0] == 1.0f);
    SDL_assert(FirstItem->Color[2] == 1.0f);

    RrArray_Push(&Handle, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    Header = (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));
    SDL_assert(Header->Count == 2);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 2);
    RrArray_Push(&Handle, &(SArrayItem){ .Color = { 1.0f, 1.0f, 1.0f, 1.0f }, .Position = { 1.0f, 1.0f, 1.0f } });
    Header = (SRrArrayHeader*)((char*)Handle - sizeof(SRrArrayHeader));
    SDL_assert(Header->Count == 3);
    SDL_assert(Header->AllocatedSize == sizeof(SArrayItem) * 4);
    SArrayItem* ThirdItem = RrArray_Get(Handle, 2);
    SDL_assert(ThirdItem->Position[0] == 1.0f);
    SDL_assert(ThirdItem->Color[2] == 1.0f);

    size_t CurrentAllocationSize = Header->AllocatedSize;
    RrArray_Empty(Handle, FALSE);
    SDL_assert(Header->Count == 0);
    SDL_assert(Header->AllocatedSize == CurrentAllocationSize);
    SDL_assert(Header->ElementSize == sizeof(SArrayItem));

    RrArray_Empty(Handle, TRUE);
    SDL_assert(InitialAllocations == SDL_GetNumAllocations());
}
#endif
