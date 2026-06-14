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

#include "Rr_Buffer.h"

#define RR_LOG_MACRO_CATEGORY RR_LOG_CATEGORY_RHI
#include "Rr_LogMacro.h"

#include "Rr_Allocator.h"
#include "Rr_RHI.h"

#include <assert.h>
#include <stdio.h>

Rr_Buffer *Rr_CreateBuffer(uint64_t Size, Rr_BufferFlags Flags)
{
    if (Size == 0)
    {
        RR_LOG_ERROR("Buffer size can't be zero!");

        return NULL;
    }

    Rr_LockSpinlock(&gRHI->BuffersLock);

    Rr_Buffer *Buffer =
        Rr_PushBufferIntoHive(&gRHI->Buffers, Rr_GetPermanent()).Element;

    Rr_UnlockSpinlock(&gRHI->BuffersLock);

    *Buffer = (Rr_Buffer){
        .Flags = Flags,
        .Size = (VkDeviceSize)Size,
    };

    Rr_ConsumeNextObjectName(Buffer->Name);

    Buffer->Usage = 0;
    if (Flags & RR_BUFFER_FLAGS_UNIFORM_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_STORAGE_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_VERTEX_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_INDEX_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if (Flags & RR_BUFFER_FLAGS_INDIRECT_BIT)
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    Buffer->AllocatedBufferCount = 1;
    if (Flags & RR_BUFFER_FLAGS_PER_FRAME_BIT)
    {
        Buffer->AllocatedBufferCount = RR_FRAME_OVERLAP;
    }

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = Size,
        .usage = Buffer->Usage,
    };

    Rr_Device *Device = Rr_GetDevice();

    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        AllocatedBuffer->SyncState = RR_EMPTY_SYNC;

        if (Device->CreateBuffer(
                Device->Handle,
                &BufferCreateInfo,
                NULL,
                &AllocatedBuffer->Handle) != VK_SUCCESS)
        {
            RR_LOG_ERROR("Failed to create buffer!");

            Rr_DestroyBuffer(Buffer);

            return NULL;
        }

#ifdef RR_USE_GPU_DEBUG_UTILS
        char ObjectName[RR_MAX_OBJECT_NAME_LENGTH];
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

    if (!Rr_AllocBufferMemory(&gRHI->Allocator, Buffer))
    {
        Rr_DestroyBuffer(Buffer);

        return NULL;
    }

    return Buffer;
}

size_t Rr_GetBufferSize(Rr_Buffer *Buffer)
{
    return (size_t)Buffer->Size;
}

void Rr_ReleaseBuffer(Rr_Buffer *Buffer)
{
    if (Buffer == NULL)
    {
        return;
    }

    Rr_LockSpinlock(&gRHI->ReleasedBuffersLock);

    Rr_InsertIntoHandleSet(
        &gRHI->ReleasedBuffers,
        (Rr_Handle const *)&Buffer,
        Rr_GetPermanent());

    Rr_UnlockSpinlock(&gRHI->ReleasedBuffersLock);
}

void Rr_DestroyBuffer(Rr_Buffer *Buffer)
{
    assert(Buffer);

    Rr_Device *Device = &gRHI->Device;

    for (uint32_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];

        if (AllocatedBuffer->Handle != VK_NULL_HANDLE)
        {
            Device->DestroyBuffer(
                Device->Handle,
                AllocatedBuffer->Handle,
                NULL);
        }
    }

    Rr_FreeBufferMemory(&gRHI->Allocator, Buffer);

    Rr_LockSpinlock(&gRHI->BuffersLock);

    Rr_BufferHiveIterator It = Rr_GetBufferHiveIterator(&gRHI->Buffers, Buffer);
    Rr_RemoveFromBufferHive(&gRHI->Buffers, &It);

    Rr_UnlockSpinlock(&gRHI->BuffersLock);
}

void *Rr_GetMappedBufferData(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    return AllocatedBuffer->MappedData;
}

void *Rr_MapBuffer(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
    {
        return AllocatedBuffer->MappedData;
    }

    return Rr_MapAllocatedBufferMemory(&gRHI->Allocator, AllocatedBuffer);
}

void Rr_UnmapBuffer(Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    if (Buffer->Flags & RR_BUFFER_FLAGS_MAPPED_BIT)
    {
        return;
    }

    Rr_UnmapAllocatedBufferMemory(&gRHI->Allocator, AllocatedBuffer);
}

void Rr_FlushBufferRange(Rr_Buffer *Buffer, uint64_t Offset, uint64_t Size)
{
    Rr_AllocatedBuffer *AllocatedBuffer = Rr_GetCurrentAllocatedBuffer(Buffer);

    Rr_FlushAllocatedBufferMemory(
        &gRHI->Allocator,
        AllocatedBuffer,
        Offset,
        Size);
}

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_Buffer *Buffer)
{
    return &Buffer->AllocatedBuffers
                [gRHI->FrameIndex % Buffer->AllocatedBufferCount];
}
