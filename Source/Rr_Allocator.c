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

void Rr_InitAllocator(
    Rr_Allocator *Allocator,
    Rr_PhysicalDevice *PhysicalDevice)
{
    VkPhysicalDeviceMemoryProperties *MemoryProperties =
        &PhysicalDevice->MemoryProperties;

    Rr_Arena *Arena = Rr_GetPermanent();

    Allocator->BufferImageGranularity =
        PhysicalDevice->Properties.limits.bufferImageGranularity;
    Allocator->NonCoherentAtomSize =
        PhysicalDevice->Properties.limits.nonCoherentAtomSize;
    Allocator->BigChunkSize = RR_BIG_CHUNK_SIZE;
    Allocator->SmallChunkSize = RR_SMALL_CHUNK_SIZE;

    Allocator->MemoryTypeCount = MemoryProperties->memoryTypeCount;
    Allocator->MemoryTypes =
        Rr_Alloc(sizeof(Rr_MemoryType) * Allocator->MemoryTypeCount, Arena);

    for (uint32_t Index = 0; Index < MemoryProperties->memoryTypeCount; ++Index)
    {
        VkMemoryType *MemoryType = &MemoryProperties->memoryTypes[Index];
        VkMemoryHeap *MemoryHeap =
            &MemoryProperties->memoryHeaps[MemoryType->heapIndex];

        Allocator->MemoryTypes[Index].HeapSize = MemoryHeap->size;
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
        Allocator->MemoryTypes[Index].DeviceLocal =
            MemoryHeap->flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT;
        Allocator->MemoryTypes[Index].HostVisible =
            MemoryType->propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT;
    }
}

void Rr_CleanupAllocator(Rr_Allocator *Allocator)
{
    Rr_Device *Device = Rr_GetDevice();

    size_t MemoryFreed = 0;
    for (size_t Index = 0; Index < Allocator->MemoryTypeCount; ++Index)
    {
        Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[Index];
        Rr_Chunk *Chunk = MemoryType->FirstChunk;

        while (Chunk)
        {
            Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

            MemoryFreed += Chunk->Size;

            Rr_DecrementAtomicIntRelaxed(&Allocator->HardAllocations);

            Chunk = Chunk->Next;
        }
    }

    Rr_ClearRangeHive(&Allocator->RangeHive);
    Rr_ClearChunkHive(&Allocator->ChunkHive);

    int64_t SoftAllocations =
        Rr_LoadAtomicIntRelaxed(&Allocator->SoftAllocations);
    if (SoftAllocations)
    {
        RR_LOG_WARNING("Leaked %zu soft allocations", (size_t)SoftAllocations);
    }
    int64_t HardAllocations =
        Rr_LoadAtomicIntRelaxed(&Allocator->HardAllocations);
    if (HardAllocations)
    {
        RR_LOG_WARNING("Leaked %zu hard allocations", (size_t)HardAllocations);
    }
    RR_LOG_INFO("Freed %zu bytes of memory", MemoryFreed);
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

static inline Rr_Chunk *Rr_GetChunk(Rr_Allocator *Allocator, Rr_Arena *Arena)
{
    return memset(
        Rr_PushChunkIntoHive(&Allocator->ChunkHive, Arena).Element,
        0,
        sizeof(Rr_Chunk));
}

static inline void Rr_ReturnChunk(Rr_Allocator *Allocator, Rr_Chunk *Chunk)
{
    Rr_ChunkHiveIterator It =
        Rr_GetChunkHiveIterator(&Allocator->ChunkHive, Chunk);
    Rr_RemoveFromChunkHive(&Allocator->ChunkHive, &It);
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

    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];

    VkMemoryAllocateInfo MemoryAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = Size,
        .memoryTypeIndex = MemoryTypeIndex,
    };
    VkDeviceMemory DeviceMemory;
    VkResult Result = Device->AllocateMemory(
        Device->Handle,
        &MemoryAllocateInfo,
        NULL,
        &DeviceMemory);
    if (Result != VK_SUCCESS)
    {
        RR_LOG_ERROR("Couldn't not allocate a chunk of memory!");

        return NULL;
    }
    Rr_IncrementAtomicIntRelaxed(&Allocator->HardAllocations);
    void *MappedData = NULL;
    if (MemoryType->HostVisible)
    {
        Result = Device->MapMemory(
            Device->Handle,
            DeviceMemory,
            0,
            Size,
            0,
            &MappedData);
        if (Result != VK_SUCCESS)
        {
            RR_LOG_ERROR("Couldn't not map a chunk of memory!");

            Device->FreeMemory(Device->Handle, DeviceMemory, NULL);

            return NULL;
        }
    }

    Rr_Arena *Arena = Rr_GetPermanent();

    Rr_Chunk *Chunk = Rr_GetChunk(Allocator, Arena);
    Chunk->Size = Size;
    Chunk->Memory = DeviceMemory;
    Chunk->MappedData = MappedData;
    Chunk->FirstRange = Rr_GetRange(Allocator, Arena);
    Chunk->FirstRange->Size = Size;
    Chunk->FirstRange->Free = true;
    Chunk->FirstFreeRange = Chunk->FirstRange;

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
    VkMemoryRequirements *MemoryRequirements,
    Rr_Chunk **OutChunk,
    Rr_Range **OutRange)
{
    Rr_MemoryType *MemoryType = &Allocator->MemoryTypes[MemoryTypeIndex];

    Rr_LockSpinlock(&Allocator->Lock);

    if (MemoryRequirements->size > MemoryType->ChunkSize)
    {
        /* Dedicated allocation. */

        Rr_Chunk *Chunk = Rr_AllocateChunk(
            Allocator,
            MemoryTypeIndex,
            MemoryRequirements->size);

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

    if (!MemoryType->FirstChunk)
    {
        MemoryType->FirstChunk =
            Rr_AllocateChunk(Allocator, MemoryTypeIndex, MemoryType->ChunkSize);

        if (!MemoryType->FirstChunk)
        {
            Rr_UnlockSpinlock(&Allocator->Lock);

            return false;
        }
    }

    Rr_Chunk *Chunk = MemoryType->FirstChunk;

    /* TODO: Technically aligning to buffer image granularity is not always
     * necessary. */

    VkDeviceSize Alignment = RR_MAX(
        MemoryRequirements->alignment,
        Allocator->BufferImageGranularity);

    while (Chunk)
    {
        Rr_Range *PreviousRange = NULL;
        Rr_Range **RangeRef = &Chunk->FirstFreeRange;
        Rr_Range *Range = Chunk->FirstFreeRange;
        while (Range)
        {
            VkDeviceSize AlignedOffset =
                Rr_AlignVulkanOffset(Range->Offset, Alignment);
            VkDeviceSize AlignedSize =
                Range->Size - (AlignedOffset - Range->Offset);
            if (MemoryRequirements->size <= AlignedSize)
            {
                /* Claim this range; put leftovers (if any) into a new one. */

                VkDeviceSize Leftovers = AlignedSize - MemoryRequirements->size;
                Rr_Range *NewRange = NULL;
                if (Leftovers >= RR_MINIMAL_ALLOCATION)
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

        if (!Chunk->Next)
        {
            Chunk->Next = Rr_AllocateChunk(
                Allocator,
                MemoryTypeIndex,
                MemoryType->ChunkSize);
        }
        Chunk = Chunk->Next;
    }

    Rr_UnlockSpinlock(&Allocator->Lock);

    return false;
}

static inline bool Rr_BindAllocatedBuffer(
    Rr_Allocator *Allocator,
    Rr_AllocatedBuffer *AllocatedBuffer,
    VkMemoryRequirements *MemoryRequirements,
    uint32_t MemoryTypeIndex)
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
            MemoryRequirements,
            &AllocatedBuffer->Chunk,
            &AllocatedBuffer->Range))
    {
        RR_LOG_ERROR("Failed to find appropriate sub allocation!");

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

    if (AllocatedBuffer->Chunk->MappedData)
    {
        AllocatedBuffer->MappedData =
            (char *)AllocatedBuffer->Chunk->MappedData +
            AllocatedBuffer->Range->AlignedOffset;
    }

    if (!AllocatedBuffer->Chunk->Dedicated)
    {
        Rr_IncrementAtomicIntRelaxed(&AllocatedBuffer->Chunk->SoftAllocations);
        Rr_IncrementAtomicIntRelaxed(&Allocator->SoftAllocations);
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
    MemoryRequirements.size =
        RR_MAX(MemoryRequirements.size, RR_MINIMAL_ALLOCATION);

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
            if (!Rr_BindAllocatedBuffer(
                    Allocator,
                    &Buffer->AllocatedBuffers[AllocatedIndex],
                    &MemoryRequirements,
                    MemoryTypeIndex))
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
    Rr_LockSpinlock(&Allocator->Lock);

    for (size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        Rr_Chunk *Chunk = AllocatedBuffer->Chunk;
        Rr_Range *Range = AllocatedBuffer->Range;
        if (!Chunk || !Range)
        {
            continue;
        }

        if (Chunk->Dedicated)
        {
            Rr_Device *Device = Rr_GetDevice();

            Device->FreeMemory(Device->Handle, Chunk->Memory, NULL);

            Rr_ReturnRange(Allocator, Range);
            Rr_ReturnChunk(Allocator, Chunk);

            Rr_DecrementAtomicIntRelaxed(&Allocator->HardAllocations);

            continue;
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

        Rr_DecrementAtomicIntRelaxed(&Chunk->SoftAllocations);
        Rr_DecrementAtomicIntRelaxed(&Allocator->SoftAllocations);
    }

    Rr_UnlockSpinlock(&Allocator->Lock);
}

void Rr_FlushBufferMemory(
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

bool Rr_AllocImageMemory(Rr_Allocator *Allocator, struct Rr_Image *Image)
{
    return false;
}

void Rr_FreeImageMemory(Rr_Allocator *Allocator, struct Rr_Image *Image)
{
}
