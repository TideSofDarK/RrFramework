/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Buffer.h"

#include "Rr_Renderer.h"

#include <assert.h>

Rr_Buffer *Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags)
{
    assert(Size > 0 && "Buffer size is zero!");

    Rr_LockSpinlock(&gRenderer->BuffersLock);

    Rr_Buffer *Buffer = Rr_PushBufferIntoHiveLocked(
                            &gRenderer->Buffers,
                            gRenderer->Arena,
                            &gRenderer->Lock)
                            .Element;

    Rr_UnlockSpinlock(&gRenderer->BuffersLock);

    *Buffer = (Rr_Buffer){
        .Flags = Flags,
    };

    Rr_ConsumeNextObjectName(Buffer->Name);

    Buffer->Usage = 0;
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_UNIFORM_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STORAGE_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_VERTEX_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_INDEX_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_INDIRECT_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    /* Fixing VMA issues with small buffers. */

    Size = RR_MAX(Size, 128);

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = Buffer->Usage,
    };

    VmaAllocationCreateInfo AllocationCreateInfo = { 0 };
    AllocationCreateInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        AllocationCreateInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_READBACK_BIT))
    {
        AllocationCreateInfo.requiredFlags |=
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        AllocationCreateInfo.flags |=
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STAGING_BIT))
    {
        AllocationCreateInfo.requiredFlags |=
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if (RR_HAS_BIT(
                AllocationCreateInfo.flags,
                VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) == false)
        {

            AllocationCreateInfo.flags |=
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }
    }
    else if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT))
    {
        AllocationCreateInfo.preferredFlags |=
            VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        AllocationCreateInfo.flags |=
            VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    Buffer->AllocatedBufferCount = 1;
    if (RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_PER_FRAME_BIT))
    {
        Buffer->AllocatedBufferCount = RR_FRAME_OVERLAP;
    }
    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        VmaAllocationInfo AllocationInfo;

        VkResult Result = vmaCreateBuffer(
            gRenderer->Allocator,
            &BufferCreateInfo,
            &AllocationCreateInfo,
            &AllocatedBuffer->Handle,
            &AllocatedBuffer->Allocation,
            &AllocationInfo);
        assert(Result == VK_SUCCESS);

        AllocatedBuffer->MappedData = AllocationInfo.pMappedData;

#ifdef RR_USE_GPU_DEBUG_UTILS
        char ObjectName[32];
        if (snprintf(
                ObjectName,
                sizeof(ObjectName) - 1,
                "%s#%d",
                Buffer->Name,
                Index))
        {
        }
        Rr_SetVulkanObjectName(
            VK_OBJECT_TYPE_BUFFER,
            (uint64_t)AllocatedBuffer->Handle,
            ObjectName);
#endif
    }

    return Buffer;
}

void Rr_ReleaseBuffer(Rr_Buffer *Buffer)
{
    if (Buffer == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRenderer->ReleasedBuffersLock);

    *Rr_PushHandleIntoHiveLocked(
         &gRenderer->ReleasedBuffers,
         gRenderer->Arena,
         &gRenderer->Lock)
         .Element = Buffer;

    Rr_UnlockSpinlock(&gRenderer->ReleasedBuffersLock);
}

void Rr_DestroyBuffer(Rr_Buffer *Buffer)
{
    assert(Buffer && Buffer->AllocatedBufferCount > 0);

    Rr_PrintDestroyMessage("Rr_Buffer", Buffer->Name, Buffer);

    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];

        Rr_LockSpinlock(&gRenderer->SyncStateStorageLock);

        Rr_EraseSyncState(
            &gRenderer->SyncStateStorage,
            (uint64_t)AllocatedBuffer->Handle);

        Rr_UnlockSpinlock(&gRenderer->SyncStateStorageLock);

        vmaDestroyBuffer(
            gRenderer->Allocator,
            AllocatedBuffer->Handle,
            AllocatedBuffer->Allocation);
    }

    Rr_LockSpinlock(&gRenderer->BuffersLock);

    Rr_BufferHiveIterator It =
        Rr_GetBufferHiveIterator(&gRenderer->Buffers, Buffer);
    Rr_RemoveFromBufferHive(&gRenderer->Buffers, &It);

    Rr_UnlockSpinlock(&gRenderer->BuffersLock);
}

void *Rr_GetMappedBufferData(Rr_Buffer *Buffer)
{
    assert(Buffer);
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);
    return AllocatedBuffer->MappedData;
}

void *Rr_MapBuffer(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);
    if (RR_HAS_BIT(Buffer->Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        return AllocatedBuffer->MappedData;
    }
    void *MappedData;
    vmaMapMemory(
        gRenderer->Allocator,
        AllocatedBuffer->Allocation,
        &MappedData);
    return MappedData;
}

void Rr_UnmapBuffer(Rr_Buffer *Buffer)
{
    if (RR_HAS_BIT(Buffer->Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        return;
    }
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);
    vmaUnmapMemory(gRenderer->Allocator, AllocatedBuffer->Allocation);
}

void Rr_FlushBufferRange(Rr_Buffer *Buffer, uint64_t Offset, uint64_t Size)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);
    vmaFlushAllocation(
        gRenderer->Allocator,
        AllocatedBuffer->Allocation,
        Offset,
        Size);
}

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_Buffer *Buffer)
{
    return &Buffer->AllocatedBuffers
                [gRenderer->FrameIndex % Buffer->AllocatedBufferCount];
}
