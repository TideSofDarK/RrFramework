#include "Rr_Buffer.h"

#include "Rr_Renderer.h"
#include "Rr_UploadContext.h"

#include <assert.h>

Rr_Buffer *Rr_CreateBuffer(
    Rr_Renderer *Renderer,
    size_t Size,
    Rr_BufferFlags Flags)
{
    Rr_Buffer *Buffer =
        RR_GET_FREE_LIST_ITEM(&Renderer->Buffers, Renderer->Arena);
    Buffer->Flags = Flags;

    Buffer->Usage = 0;
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_UNIFORM_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STORAGE_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_VERTEX_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_INDEX_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_INDIRECT_BIT))
    {
        Buffer->Usage |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    Buffer->Usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;

    /* Fixing VMA issues with small buffers. */

    Size = SDL_max(Size, 128);

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = Buffer->Usage,
    };

    VmaAllocationCreateInfo AllocationInfo = { 0 };
    AllocationInfo.usage = VMA_MEMORY_USAGE_AUTO;

    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        AllocationInfo.flags |= VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_READBACK_BIT))
    {
        AllocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        AllocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STAGING_BIT))
    {
        AllocationInfo.requiredFlags |= VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        if(RR_HAS_BIT(
               AllocationInfo.flags,
               VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT) == false)
        {

            AllocationInfo.flags |=
                VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        }
    }
    else if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_STAGING_INCOHERENT_BIT))
    {
        AllocationInfo.preferredFlags |= VK_MEMORY_PROPERTY_HOST_CACHED_BIT;
        AllocationInfo.flags |= VMA_ALLOCATION_CREATE_HOST_ACCESS_RANDOM_BIT;
    }

    Buffer->AllocatedBufferCount = 1;
    if(RR_HAS_BIT(Flags, RR_BUFFER_FLAGS_PER_FRAME_BIT))
    {
        Buffer->AllocatedBufferCount = RR_FRAME_OVERLAP;
    }
    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaCreateBuffer(
            Renderer->Allocator,
            &BufferCreateInfo,
            &AllocationInfo,
            &AllocatedBuffer->Handle,
            &AllocatedBuffer->Allocation,
            &AllocatedBuffer->AllocationInfo);
    }

    return Buffer;
}

void Rr_DestroyBuffer(Rr_Renderer *Renderer, Rr_Buffer *Buffer)
{
    if(Buffer == NULL)
    {
        return;
    }

    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaDestroyBuffer(
            Renderer->Allocator,
            AllocatedBuffer->Handle,
            AllocatedBuffer->Allocation);
    }

    RR_RETURN_FREE_LIST_ITEM(&Renderer->Buffers, Buffer);
}

void *Rr_GetMappedBufferData(Rr_Renderer *Renderer, Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer =
        Rr_GetCurrentAllocatedBuffer(Renderer, Buffer);
    return AllocatedBuffer->AllocationInfo.pMappedData;
}

void *Rr_MapBuffer(Rr_Renderer *Renderer, Rr_Buffer *Buffer)
{
    Rr_AllocatedBuffer *AllocatedBuffer =
        Rr_GetCurrentAllocatedBuffer(Renderer, Buffer);
    if(RR_HAS_BIT(Buffer->Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        return AllocatedBuffer->AllocationInfo.pMappedData;
    }
    void *MappedData;
    vmaMapMemory(Renderer->Allocator, AllocatedBuffer->Allocation, &MappedData);
    return MappedData;
}

void Rr_UnmapBuffer(Rr_Renderer *Renderer, Rr_Buffer *Buffer)
{
    if(RR_HAS_BIT(Buffer->Flags, RR_BUFFER_FLAGS_MAPPED_BIT))
    {
        return;
    }
    Rr_AllocatedBuffer *AllocatedBuffer =
        Rr_GetCurrentAllocatedBuffer(Renderer, Buffer);
    vmaUnmapMemory(Renderer->Allocator, AllocatedBuffer->Allocation);
}

void Rr_FlushBufferRange(
    Rr_Renderer *Renderer,
    Rr_Buffer *Buffer,
    size_t Offset,
    size_t Size)
{
    Rr_AllocatedBuffer *AllocatedBuffer =
        Rr_GetCurrentAllocatedBuffer(Renderer, Buffer);
    vmaFlushAllocation(
        Renderer->Allocator,
        AllocatedBuffer->Allocation,
        Offset,
        Size);
}

void Rr_UploadStagingBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Buffer *StagingBuffer,
    size_t StagingOffset,
    size_t StagingSize)
{
    Rr_Device *Device = &Renderer->Device;

    VkCommandBuffer CommandBuffer = UploadContext->CommandBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;

    for(size_t AllocatedIndex = 0;
        AllocatedIndex < Buffer->AllocatedBufferCount;
        ++AllocatedIndex)
    {
        Rr_AllocatedBuffer *AllocatedBuffer =
            Buffer->AllocatedBuffers + AllocatedIndex;

        Device->CmdPipelineBarrier(
            CommandBuffer,
            SrcState.StageMask,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            0,
            0,
            NULL,
            1,
            &(VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = SrcState.AccessMask,
                .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            },
            0,
            NULL);

        VkBufferCopy Copy = { .size = StagingSize,
                              .srcOffset = StagingOffset,
                              .dstOffset = 0 };

        Device->CmdCopyBuffer(
            CommandBuffer,
            AllocatedStagingBuffer->Handle,
            AllocatedBuffer->Handle,
            1,
            &Copy);

        if(UploadContext->UseAcquireBarriers)
        {
            *RR_PUSH_SLICE(
                &UploadContext->ReleaseBufferMemoryBarriers,
                UploadContext->Arena) = (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };

            *RR_PUSH_SLICE(
                &UploadContext->AcquireBufferMemoryBarriers,
                UploadContext->Arena) = (VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = AllocatedBuffer->Handle,
                .offset = 0,
                .size = VK_WHOLE_SIZE,
                .srcAccessMask = 0,
                .dstAccessMask = DstState.AccessMask,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
            };
        }
        else
        {
            Device->CmdPipelineBarrier(
                CommandBuffer,
                VK_PIPELINE_STAGE_TRANSFER_BIT,
                DstState.StageMask,
                0,
                0,
                NULL,
                1,
                &(VkBufferMemoryBarrier){
                    .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                    .pNext = NULL,
                    .buffer = AllocatedBuffer->Handle,
                    .offset = 0,
                    .size = VK_WHOLE_SIZE,
                    .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                    .dstAccessMask = DstState.AccessMask,
                    .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                    .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                },
                0,
                NULL);
        }
    }
}

void Rr_UploadBuffer(
    Rr_Renderer *Renderer,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    Rr_SyncState SrcState,
    Rr_SyncState DstState,
    Rr_Data Data)
{
    Rr_Buffer *StagingBuffer = Rr_CreateBuffer(
        Renderer,
        Data.Size,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    *RR_PUSH_SLICE(&UploadContext->StagingBuffers, UploadContext->Arena) =
        StagingBuffer;

    Rr_AllocatedBuffer *AllocatedStagingBuffer =
        StagingBuffer->AllocatedBuffers;
    memcpy(
        AllocatedStagingBuffer->AllocationInfo.pMappedData,
        Data.Pointer,
        Data.Size);

    Rr_UploadStagingBuffer(
        Renderer,
        UploadContext,
        Buffer,
        SrcState,
        DstState,
        StagingBuffer,
        0,
        Data.Size);
}

void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer *Renderer,
    Rr_Buffer *DstBuffer,
    Rr_Data Data)
{
    assert(DstBuffer != NULL);

    Rr_Device *Device = &Renderer->Device;

    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);

    Rr_Buffer *SrcBuffer = Rr_CreateBuffer(
        Renderer,
        Data.Size,
        RR_BUFFER_FLAGS_STAGING_BIT | RR_BUFFER_FLAGS_MAPPED_BIT);
    Rr_AllocatedBuffer *SrcAllocatedBuffer =
        Rr_GetCurrentAllocatedBuffer(Renderer, SrcBuffer);
    memcpy(
        SrcAllocatedBuffer->AllocationInfo.pMappedData,
        Data.Pointer,
        Data.Size);

    VkBufferCopy BufferCopy = {
        .dstOffset = 0,
        .size = Data.Size,
        .srcOffset = 0,
    };

    for(size_t Index = 0; Index < DstBuffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *DstAllocatedBuffer =
            &DstBuffer->AllocatedBuffers[Index];
        Device->CmdCopyBuffer(
            CommandBuffer,
            SrcAllocatedBuffer->Handle,
            DstAllocatedBuffer->Handle,
            1,
            &BufferCopy);
    }

    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(Renderer, SrcBuffer);
}

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(
    Rr_Renderer *Renderer,
    Rr_Buffer *Buffer)
{
    size_t AllocatedBufferIndex =
        Renderer->CurrentFrameIndex % Buffer->AllocatedBufferCount;
    return &Buffer->AllocatedBuffers[AllocatedBufferIndex];
}
