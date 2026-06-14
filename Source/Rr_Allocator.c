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

#include "Rr_Allocator.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RHI
#include "Rr_LogMacro.h"

#include "Rr_RHI.h"

#define RR_MIN_LEFTOVERS_SIZE 1024

/* TODO: Per memory type locking? */

void Rr_InitAllocator(
    Rr_Allocator *Allocator,
    Rr_PhysicalDevice *PhysicalDevice)
{
    uint32_t MemoryTypeCount = PhysicalDevice->MemoryProperties.memoryTypeCount;
    VkMemoryType *MemoryTypes = PhysicalDevice->MemoryProperties.memoryTypes;
    VkMemoryHeap *MemoryHeaps = PhysicalDevice->MemoryProperties.memoryHeaps;
    VkPhysicalDeviceLimits *Limits = &PhysicalDevice->Properties.limits;

    Rr_Arena *Arena = Rr_GetPermanent();

    Allocator->BufferImageGranularity = Limits->bufferImageGranularity;
    Allocator->NonCoherentAtomSize = Limits->nonCoherentAtomSize;
    Allocator->BigChunkSize = RR_BIG_CHUNK_SIZE;
    Allocator->SmallChunkSize = RR_SMALL_CHUNK_SIZE;

    Allocator->MemoryTypeCount = MemoryTypeCount;
    Allocator->MemoryTypes =
        Rr_Alloc(sizeof(Rr_MemoryType) * MemoryTypeCount, Arena);

    for (uint32_t Index = 0; Index < MemoryTypeCount; ++Index)
    {
        VkMemoryType *MemoryType = &MemoryTypes[Index];
        VkMemoryHeap *MemoryHeap = &MemoryHeaps[MemoryType->heapIndex];

        Allocator->MemoryTypes[Index].HeapSize = MemoryHeap->size;
        Allocator->MemoryTypes[Index].DeviceLocalHeap =
            MemoryHeap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        if (MemoryHeap->size > Allocator->BigChunkSize)
        {
            Allocator->MemoryTypes[Index].ChunkSize = Allocator->BigChunkSize;
        }
        else if (MemoryHeap->size > Allocator->SmallChunkSize)
        {
            Allocator->MemoryTypes[Index].ChunkSize = Allocator->SmallChunkSize;
        }
        else
        {
            Allocator->MemoryTypes[Index].ChunkSize = MemoryHeap->size / 2;
        }
        Allocator->MemoryTypes[Index].PropertyFlags = MemoryType->propertyFlags;
    }
}

void Rr_CleanupAllocator(Rr_Allocator *Allocator)
{
    Rr_Device *Device = Rr_GetDevice();

    Rr_LockSpinlock(&Allocator->Lock);

    uint32_t LeakedMappings = 0;
    size_t DeviceLocalMemoryFreed = 0;
    for (size_t Index = 0; Index < Allocator->MemoryTypeCount; ++Index)
    {
        Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[Index];

        Rr_ChunkHiveIterator It = MemoryType->ChunkHive.Begin;
        while (It.Element != MemoryType->ChunkHive.End.Element)
        {
            Rr_Chunk *Chunk = It.Element;

            if (Chunk->MappedData)
            {
                Device->UnmapMemory(Device->Handle, Chunk->Memory);
            }

            Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

            LeakedMappings += Chunk->MappingCount;

            if (MemoryType->DeviceLocalHeap)
            {
                DeviceLocalMemoryFreed += Chunk->Size;
            }

            Allocator->HardAllocationCount--;

            Rr_AdvanceChunkHiveIterator(&It);
        }

        Rr_ClearChunkHive(&MemoryType->ChunkHive);
    }

    Rr_ClearRangeHive(&Allocator->RangeHive);

    if (Allocator->SoftAllocationCount)
    {
        RR_LOG_WARNING(
            "Leaked %u soft allocations",
            Allocator->SoftAllocationCount);
    }
    if (Allocator->HardAllocationCount)
    {
        RR_LOG_WARNING(
            "Leaked %u hard allocations",
            Allocator->HardAllocationCount);
    }
    if (LeakedMappings)
    {
        RR_LOG_WARNING("Leaked %u memory mappings", LeakedMappings);
    }
    if (DeviceLocalMemoryFreed)
    {
        RR_LOG_INFO(
            "Freed %.2f mebibytes of pooled device local memory",
            (double)DeviceLocalMemoryFreed / (double)RR_MEBIBYTES(1));
    }

    Rr_UnlockSpinlock(&Allocator->Lock);
}

static inline Rr_Range *Rr_GetRange(Rr_Allocator *Allocator, Rr_Arena *Arena)
{
    return memset(
        Rr_PushRangeIntoHive(&Allocator->RangeHive, Arena).Element,
        0,
        sizeof(Rr_Range));
}

static inline void Rr_ReturnRange(Rr_Allocator *Allocator, Rr_Range *Range)
{
    Rr_RangeHiveIterator It =
        Rr_GetRangeHiveIterator(&Allocator->RangeHive, Range);
    Rr_RemoveFromRangeHive(&Allocator->RangeHive, &It);
}

static inline uint32_t Rr_FindMemoryType(
    Rr_Allocator *Allocator,
    uint32_t Filter,
    VkMemoryPropertyFlags RequiredFlags,
    VkMemoryPropertyFlags PreferredFlags)
{
    VkPhysicalDeviceMemoryProperties *MemoryProperties =
        &gRHI->PhysicalDevice.MemoryProperties;

    /* First pass: required and preferred. */

    for (uint32_t Index = 0; Index < MemoryProperties->memoryTypeCount; ++Index)
    {
        bool PassesFilter = Filter & (1 << Index);
        if (!PassesFilter)
        {
            continue;
        }

        VkMemoryType *MemoryType = &MemoryProperties->memoryTypes[Index];

        if ((MemoryType->propertyFlags & RequiredFlags) == RequiredFlags &&
            (MemoryType->propertyFlags & PreferredFlags) == PreferredFlags)
        {
            return Index;
        }
    }

    /* Second pass: required only. */

    for (uint32_t Index = 0; Index < MemoryProperties->memoryTypeCount; ++Index)
    {
        bool PassesFilter = Filter & (1 << Index);
        if (!PassesFilter)
        {
            continue;
        }

        VkMemoryType *MemoryType = &MemoryProperties->memoryTypes[Index];

        if ((MemoryType->propertyFlags & RequiredFlags) == RequiredFlags)
        {
            return Index;
        }
    }

    return VK_MAX_MEMORY_TYPES;
}

static inline Rr_Chunk *Rr_AllocateChunk(
    Rr_Allocator *Allocator,
    uint32_t MemoryTypeIndex,
    VkDeviceSize Size)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryAllocateInfo MemoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = Size,
        .memoryTypeIndex = MemoryTypeIndex,
    };
    VkDeviceMemory DeviceMemory;
    if (Device->AllocateMemory(
            Device->Handle,
            &MemoryAllocateInfo,
            NULL,
            &DeviceMemory) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Couldn't not allocate a chunk of memory!");

        return NULL;
    }
    Allocator->HardAllocationCount++;

    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];
    Rr_Arena *Arena = Rr_GetPermanent();

    Rr_Range *Range = Rr_GetRange(Allocator, Arena);
    *Range = (Rr_Range){
        .Size = Size,
        .Free = true,
    };

    Rr_Chunk *Chunk =
        Rr_PushChunkIntoHive(&MemoryType->ChunkHive, Arena).Element;
    *Chunk = (Rr_Chunk){
        .Size = Size,
        .Memory = DeviceMemory,
        .MemoryTypeIndex = MemoryTypeIndex,
        .FirstRange = Range,
        .FirstFreeRange = Range,
    };

    return Chunk;
}

static inline VkDeviceSize Rr_AlignVulkanOffset(
    VkDeviceSize Offset,
    VkDeviceSize Alignment)
{
    if (Offset % Alignment == 0)
    {
        return Offset;
    }

    return (Offset / Alignment) * Alignment + Alignment;
}

static inline bool Rr_FindChunkAndRange(
    Rr_Allocator *Allocator,
    uint32_t MemoryTypeIndex,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    Rr_Chunk **OutChunk,
    Rr_Range **OutRange)
{
    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];

    Rr_LockSpinlock(&Allocator->Lock);

    if (Size > MemoryType->ChunkSize)
    {
        /* NOTE: Dedicated allocation. */

        Rr_Chunk *Chunk = Rr_AllocateChunk(Allocator, MemoryTypeIndex, Size);

        if (!Chunk)
        {
            Rr_UnlockSpinlock(&Allocator->Lock);

            return false;
        }

        Chunk->Dedicated = true;
        Chunk->FirstFreeRange = NULL;
        Chunk->FirstRange->Free = false;

        *OutChunk = Chunk;
        *OutRange = Chunk->FirstRange;

        Rr_UnlockSpinlock(&Allocator->Lock);

        return true;
    }

    Rr_ChunkHiveIterator It = MemoryType->ChunkHive.Begin;
    while (true)
    {
        Rr_Chunk *Chunk = It.Element;

        if (It.Element == MemoryType->ChunkHive.End.Element)
        {
            Chunk = Rr_AllocateChunk(
                Allocator,
                MemoryTypeIndex,
                MemoryType->ChunkSize);

            if (!Chunk)
            {
                Rr_UnlockSpinlock(&Allocator->Lock);

                return false;
            }
        }
        else if (Chunk->Dedicated)
        {
            Rr_AdvanceChunkHiveIterator(&It);

            continue;
        }

        Rr_Range *PreviousRange = NULL;
        Rr_Range **RangeRef = &Chunk->FirstFreeRange;
        Rr_Range *Range = Chunk->FirstFreeRange;
        while (Range)
        {
            VkDeviceSize AlignedOffset =
                Rr_AlignVulkanOffset(Range->Offset, Alignment);
            VkDeviceSize AlignedDelta = AlignedOffset - Range->Offset;
            VkDeviceSize AlignedAvailableSize = Range->Size - AlignedDelta;
            if (Size <= AlignedAvailableSize)
            {
                /* Claim this range; put leftovers (if any) into a new one. */

                VkDeviceSize Leftovers = AlignedAvailableSize - Size;
                Rr_Range *NewRange = NULL;
                if (Leftovers >= RR_MIN_LEFTOVERS_SIZE)
                {
                    Range->Size -= Leftovers;

                    NewRange = Rr_GetRange(Allocator, Rr_GetPermanent());
                    NewRange->Offset = Range->Offset + Range->Size;
                    NewRange->Size = Leftovers;
                    NewRange->Free = true;

                    NewRange->Previous = Range;
                    NewRange->Next = Range->Next;

                    NewRange->PreviousFree = PreviousRange;
                    NewRange->NextFree = Range->NextFree;

                    Range->Next = NewRange;
                }

                *RangeRef = NewRange ? NewRange : Range->NextFree;

                Range->AlignedOffset = AlignedOffset;
                Range->Free = false;
                Range->PreviousFree = NULL;
                Range->NextFree = NULL;

                Rr_UnlockSpinlock(&Allocator->Lock);

                *OutChunk = Chunk;
                *OutRange = Range;

                return true;
            }

            PreviousRange = Range;
            RangeRef = &Range->NextFree;
            Range = *RangeRef;
        }

        Rr_AdvanceChunkHiveIterator(&It);
    }

    Rr_UnlockSpinlock(&Allocator->Lock);

    return false;
}

static inline void Rr_FreeChunkAndRange(
    Rr_Allocator *Allocator,
    Rr_Chunk *Chunk,
    Rr_Range *Range)
{
    Rr_LockSpinlock(&Allocator->Lock);

    if (Chunk->Dedicated)
    {
        Rr_Device *Device = Rr_GetDevice();

        Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

        Rr_ReturnRange(Allocator, Range);

        Rr_MemoryType *MemoryType =
            &Allocator->MemoryTypes[Chunk->MemoryTypeIndex];
        Rr_ChunkHiveIterator It =
            Rr_GetChunkHiveIterator(&MemoryType->ChunkHive, Chunk);
        Rr_RemoveFromChunkHive(&MemoryType->ChunkHive, &It);

        Allocator->HardAllocationCount--;

        Rr_UnlockSpinlock(&Allocator->Lock);

        return;
    }

    Range->Free = true;

    /* Coalesce in both directions. */

    Rr_Range *RangeToTheLeft = Range->Previous;
    if (RangeToTheLeft && RangeToTheLeft->Free)
    {
        Range->Offset = RangeToTheLeft->Offset;
        Range->Size += RangeToTheLeft->Size;

        Range->Previous = RangeToTheLeft->Previous;
        Range->PreviousFree = RangeToTheLeft->PreviousFree;

        if (Chunk->FirstFreeRange == RangeToTheLeft)
        {
            Chunk->FirstFreeRange = Range;
        }

        if (Chunk->FirstRange == RangeToTheLeft)
        {
            Chunk->FirstRange = Range;
        }

        Rr_ReturnRange(Allocator, RangeToTheLeft);
    }

    Rr_Range *RangeToTheRight = Range->Next;
    if (RangeToTheRight && RangeToTheRight->Free)
    {
        Range->Size += RangeToTheRight->Size;

        Range->Next = RangeToTheRight->Next;
        Range->NextFree = RangeToTheRight->NextFree;

        if (Chunk->FirstFreeRange == RangeToTheRight)
        {
            Chunk->FirstFreeRange = Range;
        }

        Rr_ReturnRange(Allocator, RangeToTheRight);
    }

    if (Range->Next)
    {
        Range->Next->Previous = Range;
    }
    if (Range->Previous)
    {
        Range->Previous->Next = Range;
    }
    if (Range->NextFree)
    {
        Range->NextFree->PreviousFree = Range;
    }
    if (Range->PreviousFree)
    {
        Range->PreviousFree->NextFree = Range;
    }

    Chunk->SoftAllocationCount--;
    Allocator->SoftAllocationCount--;

    Rr_UnlockSpinlock(&Allocator->Lock);
}

static inline bool Rr_BindAllocatedBuffer(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    uint32_t MemoryTypeIndex,
    bool Mapped)
{
    if (AllocatedBuffer->Chunk || AllocatedBuffer->Range)
    {
        RR_LOG_ERROR("Buffer memory is already bound!");

        return false;
    }

    Rr_Device *Device = Rr_GetDevice();

    if (!Rr_FindChunkAndRange(
            Allocator,
            MemoryTypeIndex,
            Size,
            Alignment,
            &AllocatedBuffer->Chunk,
            &AllocatedBuffer->Range))
    {
        RR_LOG_ERROR("Failed to find appropriate sub allocation!");

        return false;
    }

    if (Mapped && !Rr_MapAllocatedBufferMemory(Allocator, AllocatedBuffer))
    {
        return false;
    }

    if (Device->BindBufferMemory(
            Device->Handle,
            AllocatedBuffer->Handle,
            AllocatedBuffer->Chunk->Memory,
            AllocatedBuffer->Range->AlignedOffset) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to bind buffer memory!");

        return false;
    }

    if (!AllocatedBuffer->Chunk->Dedicated)
    {
        Rr_LockSpinlock(&Allocator->Lock);

        AllocatedBuffer->Chunk->SoftAllocationCount++;
        Allocator->SoftAllocationCount++;

        Rr_UnlockSpinlock(&Allocator->Lock);
    }

    return true;
}

bool Rr_AllocBufferMemory(Rr_Allocator *Allocator, Rr_Buffer *Buffer)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryRequirements MemoryRequirements;
    Device->GetBufferMemoryRequirements(
        Device->Handle,
        Buffer->AllocatedBuffers[0].Handle,
        &MemoryRequirements);
    if (MemoryRequirements.size == 0)
    {
        RR_LOG_ERROR("Invalid memory requirements for buffer!");

        return false;
    }

    VkMemoryPropertyFlags RequiredFlags = 0;
    VkMemoryPropertyFlags PreferredFlags = 0;

    if (Buffer->Flags & RR_BUFFER_FLAGS_READBACK_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        PreferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_STAGING_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT)
    {
        RequiredFlags |= VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
    if (Buffer->Flags & RR_BUFFER_FLAGS_UNIFORM_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_STORAGE_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_VERTEX_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_INDEX_BIT ||
        Buffer->Flags & RR_BUFFER_FLAGS_INDIRECT_BIT)
    {
        PreferredFlags |= VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
    }

    size_t AllocatedIndex = 0;
    uint32_t MemoryTypeFilter = MemoryRequirements.memoryTypeBits;
    while (true)
    {
        /* NOTE: This loop allows to allocate from different memory types. */

        uint32_t MemoryTypeIndex = Rr_FindMemoryType(
            Allocator,
            MemoryTypeFilter,
            RequiredFlags,
            PreferredFlags);
        if (MemoryTypeIndex == VK_MAX_MEMORY_TYPES)
        {
            RR_LOG_ERROR("Failed to find appropriate memory type!");

            break;
        }

        for (; AllocatedIndex < Buffer->AllocatedBufferCount; ++AllocatedIndex)
        {
            Rr_AllocatedBuffer *AllocatedBuffer =
                &Buffer->AllocatedBuffers[AllocatedIndex];
            if (!Rr_BindAllocatedBuffer(
                    Allocator,
                    AllocatedBuffer,
                    MemoryRequirements.size,
                    MemoryRequirements.alignment,
                    MemoryTypeIndex,
                    Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT))
            {
                break;
            }
        }

        if (AllocatedIndex == Buffer->AllocatedBufferCount)
        {
            return true;
        }

        MemoryTypeFilter &= ~(1U << MemoryTypeIndex);
    }

    Rr_FreeBufferMemory(Allocator, Buffer);

    return false;
}

void Rr_FreeBufferMemory(Rr_Allocator *Allocator, Rr_Buffer *Buffer)
{
    for (size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        Rr_Chunk *Chunk = AllocatedBuffer->Chunk;
        Rr_Range *Range = AllocatedBuffer->Range;
        if (!Chunk || !Range)
        {
            continue;
        }

        if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
        {
            Rr_UnmapAllocatedBufferMemory(Allocator, AllocatedBuffer);
        }

        Rr_FreeChunkAndRange(Allocator, Chunk, Range);
    }
}

void *Rr_MapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer)
{
    if (AllocatedBuffer->MappedData)
    {
        return AllocatedBuffer->MappedData;
    }

    Rr_Device *Device = Rr_GetDevice();

    Rr_Chunk *Chunk = AllocatedBuffer->Chunk;
    Rr_Range *Range = AllocatedBuffer->Range;

    Rr_LockSpinlock(&Allocator->Lock);

    if (!Chunk->MappedData)
    {
        if (Device->MapMemory(
                Device->Handle,
                Chunk->Memory,
                0,
                VK_WHOLE_SIZE,
                0,
                &Chunk->MappedData) != VK_SUCCESS)
        {
            Rr_UnlockSpinlock(&Allocator->Lock);

            RR_LOG_ERROR("Failed to map a chunk of memory!");

            return NULL;
        }
    }

    AllocatedBuffer->MappedData =
        (uint8_t *)Chunk->MappedData + Range->AlignedOffset;

    Chunk->MappingCount++;

    Rr_UnlockSpinlock(&Allocator->Lock);

    return (uint8_t *)Chunk->MappedData + Range->AlignedOffset;
}

void Rr_UnmapAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer)
{
    if (!AllocatedBuffer->MappedData)
    {
        return;
    }

    Rr_Chunk *Chunk = AllocatedBuffer->Chunk;

    Rr_LockSpinlock(&Allocator->Lock);

    if (Chunk->MappedData)
    {
        AllocatedBuffer->MappedData = NULL;

        if (--Chunk->MappingCount == 0)
        {
            Rr_Device *Device = Rr_GetDevice();

            Device->UnmapMemory(Device->Handle, Chunk->Memory);

            Chunk->MappedData = NULL;
        }
    }

    Rr_UnlockSpinlock(&Allocator->Lock);
}

void Rr_FlushAllocatedBufferMemory(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer,
    size_t Offset,
    size_t Size)
{
    Rr_Device *Device = Rr_GetDevice();

    if (Device->FlushMappedMemoryRanges(
            Device->Handle,
            1,
            &(VkMappedMemoryRange){
                .sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE,
                .memory = AllocatedBuffer->Chunk->Memory,
                .offset = AllocatedBuffer->Range->AlignedOffset,
                .size = Size,
            }) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to flush buffer memory!");
    }
}

static inline bool Rr_BindAllocatedImage(
    Rr_Allocator *Allocator,
    Rr_AllocatedImage *AllocatedImage,
    VkDeviceSize Size,
    VkDeviceSize Alignment,
    uint32_t MemoryTypeIndex)
{
    if (AllocatedImage->Chunk || AllocatedImage->Range)
    {
        RR_LOG_ERROR("Image memory is already bound!");

        return false;
    }

    Rr_Device *Device = Rr_GetDevice();

    if (!Rr_FindChunkAndRange(
            Allocator,
            MemoryTypeIndex,
            Size,
            Alignment,
            &AllocatedImage->Chunk,
            &AllocatedImage->Range))
    {
        RR_LOG_ERROR("Failed to find appropriate sub allocation!");

        return false;
    }

    if (Device->BindImageMemory(
            Device->Handle,
            AllocatedImage->Handle,
            AllocatedImage->Chunk->Memory,
            AllocatedImage->Range->AlignedOffset) != VK_SUCCESS)
    {
        RR_LOG_ERROR("Failed to bind image memory!");

        return false;
    }

    if (!AllocatedImage->Chunk->Dedicated)
    {
        Rr_LockSpinlock(&Allocator->Lock);

        AllocatedImage->Chunk->SoftAllocationCount++;
        Allocator->SoftAllocationCount++;

        Rr_UnlockSpinlock(&Allocator->Lock);
    }

    return true;
}

bool Rr_AllocImageMemory(Rr_Allocator *Allocator, Rr_Image *Image)
{
    Rr_Device *Device = Rr_GetDevice();

    VkMemoryRequirements MemoryRequirements;
    Device->GetImageMemoryRequirements(
        Device->Handle,
        Image->AllocatedImages[0].Handle,
        &MemoryRequirements);
    if (MemoryRequirements.size == 0)
    {
        RR_LOG_ERROR("Invalid memory requirements for image!");

        return false;
    }

    VkMemoryPropertyFlags RequiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;

    size_t AllocatedIndex = 0;
    uint32_t MemoryTypeFilter = MemoryRequirements.memoryTypeBits;
    while (true)
    {
        /* NOTE: This loop allows to allocate from different memory types. */

        uint32_t MemoryTypeIndex =
            Rr_FindMemoryType(Allocator, MemoryTypeFilter, RequiredFlags, 0);
        if (MemoryTypeIndex == VK_MAX_MEMORY_TYPES)
        {
            RR_LOG_ERROR("Failed to find appropriate memory type!");

            break;
        }

        for (; AllocatedIndex < Image->AllocatedImageCount; ++AllocatedIndex)
        {
            Rr_AllocatedImage *AllocatedImage =
                &Image->AllocatedImages[AllocatedIndex];
            if (!Rr_BindAllocatedImage(
                    Allocator,
                    AllocatedImage,
                    RR_ALIGN_POW2(
                        MemoryRequirements.size,
                        Allocator->BufferImageGranularity),
                    RR_MAX(
                        MemoryRequirements.alignment,
                        Allocator->BufferImageGranularity),
                    MemoryTypeIndex))
            {
                break;
            }
        }

        if (AllocatedIndex == Image->AllocatedImageCount)
        {
            return true;
        }

        MemoryTypeFilter &= ~(1U << MemoryTypeIndex);
    }

    Rr_FreeImageMemory(Allocator, Image);

    return false;
}

void Rr_FreeImageMemory(Rr_Allocator *Allocator, Rr_Image *Image)
{
    for (size_t Index = 0; Index < Image->AllocatedImageCount; ++Index)
    {
        Rr_AllocatedImage *AllocatedImage = &Image->AllocatedImages[Index];
        Rr_Chunk *Chunk = AllocatedImage->Chunk;
        Rr_Range *Range = AllocatedImage->Range;
        if (!Chunk || !Range)
        {
            continue;
        }

        Rr_FreeChunkAndRange(Allocator, Chunk, Range);
    }
}
