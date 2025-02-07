#include "Rr_Buffer.h"

#include "Rr_App.h"
#include "Rr_Log.h"
#include "Rr_UploadContext.h"

#include <assert.h>

Rr_Buffer *Rr_CreateBuffer_Internal(
    Rr_App *App,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool CreateMapped,
    bool Buffered)
{
    Rr_Buffer *Buffer = RR_GET_FREE_LIST_ITEM(&App->Renderer.Buffers, App->PermanentArena);
    Buffer->Usage = UsageFlags;

    /* Fixing VMA issues with small buffers. */
    Size = SDL_max(Size, 128);

    VkBufferCreateInfo BufferCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags,
    };

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = MemoryUsage,
    };

    if(CreateMapped)
    {
        AllocationInfo.flags =
            VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    Buffer->AllocatedBufferCount = Buffered ? RR_FRAME_OVERLAP : 1;
    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaCreateBuffer(
            App->Renderer.Allocator,
            &BufferCreateInfo,
            &AllocationInfo,
            &AllocatedBuffer->Handle,
            &AllocatedBuffer->Allocation,
            &AllocatedBuffer->AllocationInfo);
    }

    return Buffer;
}

Rr_Buffer *Rr_CreateBuffer(Rr_App *App, size_t Size, Rr_BufferUsage Usage, bool Buffered)
{
    return Rr_CreateBuffer_Internal(
        App,
        Size,
        Rr_GetVulkanBufferUsage(Usage) | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_GPU_ONLY,
        false,
        Buffered);
}

void Rr_DestroyBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    if(Buffer == NULL)
    {
        return;
    }

    Rr_Renderer *Renderer = &App->Renderer;

    for(size_t Index = 0; Index < Buffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *AllocatedBuffer = &Buffer->AllocatedBuffers[Index];
        vmaDestroyBuffer(Renderer->Allocator, AllocatedBuffer->Handle, AllocatedBuffer->Allocation);
    }

    RR_RETURN_FREE_LIST_ITEM(&App->Renderer.Buffers, Buffer);
}

void Rr_UploadBufferAligned(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *DstBuffer,
    VkDeviceSize *DstOffset,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    Rr_Data Data,
    size_t Alignment)
{
    // Rr_Renderer *Renderer = &App->Renderer;
    //
    // Rr_WriteBuffer *StagingBuffer = UploadContext->StagingBuffer;
    // if(StagingBuffer->Offset + Data.Size > StagingBuffer->Buffer->AllocationInfo.size)
    // {
    //     RR_ABORT(
    //         "Exceeding staging buffer size! Current offset is %zu, allocation "
    //         "size is %zu and total staging buffer size is %zu.",
    //         (size_t)StagingBuffer->Offset,
    //         Data.Size,
    //         (size_t)StagingBuffer->Buffer->AllocationInfo.size);
    // }
    //
    // VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;
    //
    // size_t StagingBufferOffset = StagingBuffer->Offset;
    // memcpy((char *)StagingBuffer->Buffer->AllocationInfo.pMappedData + StagingBufferOffset, Data.Pointer, Data.Size);
    // if(Alignment == 0)
    // {
    //     StagingBuffer->Offset += Data.Size;
    // }
    // else
    // {
    //     StagingBuffer->Offset += RR_ALIGN_POW2(StagingBufferOffset + Data.Size, Alignment);
    // }
    //
    // /* Advance DstOffset here. */
    //
    // size_t AlignedSize = RR_ALIGN_POW2(Data.Size, Alignment);
    // VkDeviceSize CurrentDstOffset = DstOffset != NULL ? *DstOffset : 0;
    // if(CurrentDstOffset + AlignedSize > DstBuffer->AllocationInfo.size)
    // {
    //     RR_ABORT(
    //         "Exceeding buffer size! Current offset is %zu, allocation size is "
    //         "%zu and total  buffer size is %zu.",
    //         CurrentDstOffset,
    //         AlignedSize,
    //         DstBuffer->AllocationInfo.size);
    // }
    // if(DstOffset != NULL)
    // {
    //     *DstOffset = RR_ALIGN_POW2(*DstOffset + AlignedSize, Alignment);
    // }
    //
    // vkCmdPipelineBarrier(
    //     TransferCommandBuffer,
    //     SrcStageMask,
    //     VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     0,
    //     0,
    //     NULL,
    //     1,
    //     &(VkBufferMemoryBarrier){
    //         .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //         .pNext = NULL,
    //         .buffer = DstBuffer->Handle,
    //         .offset = CurrentDstOffset,
    //         .size = Data.Size,
    //         .srcAccessMask = SrcAccessMask,
    //         .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    //         .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //     },
    //     0,
    //     NULL);
    //
    // VkBufferCopy Copy = { .size = Data.Size, .srcOffset = StagingBufferOffset, .dstOffset = CurrentDstOffset };
    //
    // vkCmdCopyBuffer(TransferCommandBuffer, StagingBuffer->Buffer->Handle, DstBuffer->Handle, 1, &Copy);
    //
    // if(!UploadContext->UseAcquireBarriers)
    // {
    //     vkCmdPipelineBarrier(
    //         TransferCommandBuffer,
    //         VK_PIPELINE_STAGE_TRANSFER_BIT,
    //         DstStageMask,
    //         0,
    //         0,
    //         NULL,
    //         1,
    //         &(VkBufferMemoryBarrier){
    //             .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //             .pNext = NULL,
    //             .buffer = DstBuffer->Handle,
    //             .offset = CurrentDstOffset,
    //             .size = Data.Size,
    //             .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    //             .dstAccessMask = DstAccessMask,
    //             .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //             .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
    //         },
    //         0,
    //         NULL);
    // }
    // else
    // {
    //     UploadContext->ReleaseBarriers.BufferMemoryBarriers[UploadContext->AcquireBarriers.BufferMemoryBarrierCount]
    //     =
    //         (VkBufferMemoryBarrier){
    //             .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //             .pNext = NULL,
    //             .buffer = DstBuffer->Handle,
    //             .offset = CurrentDstOffset,
    //             .size = Data.Size,
    //             .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
    //             .dstAccessMask = 0,
    //             .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
    //             .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    //         };
    //     UploadContext->ReleaseBarriers.BufferMemoryBarrierCount++;
    //
    //     UploadContext->AcquireBarriers.BufferMemoryBarriers[UploadContext->AcquireBarriers.BufferMemoryBarrierCount]
    //     =
    //         (VkBufferMemoryBarrier){
    //             .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
    //             .pNext = NULL,
    //             .buffer = DstBuffer->Handle,
    //             .offset = CurrentDstOffset,
    //             .size = Data.Size,
    //             .srcAccessMask = 0,
    //             .dstAccessMask = DstAccessMask,
    //             .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
    //             .dstQueueFamilyIndex = Renderer->GraphicsQueue.FamilyIndex,
    //         };
    //     UploadContext->AcquireBarriers.BufferMemoryBarrierCount++;
    // }
}

void Rr_UploadBuffer(
    Rr_App *App,
    Rr_UploadContext *UploadContext,
    Rr_Buffer *Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    Rr_Data Data)
{
    Rr_UploadBufferAligned(
        App,
        UploadContext,
        Buffer,
        NULL,
        SrcStageMask,
        SrcAccessMask,
        DstStageMask,
        DstAccessMask,
        Data,
        0);
}

void Rr_UploadToDeviceBufferImmediate(Rr_App *App, Rr_Buffer *DstBuffer, Rr_Data Data)
{
    assert(DstBuffer != NULL);

    Rr_Renderer *Renderer = &App->Renderer;

    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);

    Rr_Buffer *SrcBuffer = Rr_CreateBuffer_Internal(
        App,
        Data.Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_HOST,
        true,
        false);
    Rr_AllocatedBuffer *SrcAllocatedBuffer = Rr_GetCurrentAllocatedBuffer(App, SrcBuffer);
    memcpy(SrcAllocatedBuffer->AllocationInfo.pMappedData, Data.Pointer, Data.Size);

    VkBufferCopy BufferCopy = { .dstOffset = 0, .size = Data.Size, .srcOffset = 0 };
    for(size_t Index = 0; Index < DstBuffer->AllocatedBufferCount; ++Index)
    {
        Rr_AllocatedBuffer *DstAllocatedBuffer = &DstBuffer->AllocatedBuffers[Index];
        vkCmdCopyBuffer(CommandBuffer, SrcAllocatedBuffer->Handle, DstAllocatedBuffer->Handle, 1, &BufferCopy);
    }

    Rr_EndImmediate(Renderer);

    Rr_DestroyBuffer(App, SrcBuffer);
}

// void Rr_UploadToUniformBuffer(
//     Rr_App *App,
//     Rr_UploadContext *UploadContext,
//     Rr_Buffer *DstBuffer,
//     VkDeviceSize *DstOffset,
//     Rr_Data Data)
// {
//     Rr_UploadBufferAligned(
//         App,
//         UploadContext,
//         DstBuffer,
//         DstOffset,
//         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//         VK_ACCESS_UNIFORM_READ_BIT,
//         VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//         VK_ACCESS_UNIFORM_READ_BIT,
//         Data,
//         Rr_GetUniformAlignment(&App->Renderer));
// }

// void Rr_CopyToMappedUniformBuffer(Rr_App *App, Rr_Buffer *DstBuffer, VkDeviceSize *DstOffset, Rr_Data Data)
// {
//     uint32_t Alignment = App->Renderer.PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
//     size_t AlignedSize = RR_ALIGN_POW2(Data.Size, Alignment);
//     if(*DstOffset + AlignedSize <= DstBuffer->AllocationInfo.size)
//     {
//         memcpy((char *)DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data.Pointer, Data.Size);
//         *DstOffset += Data.Size;
//         *DstOffset = RR_ALIGN_POW2(*DstOffset, Alignment);
//     }
//     else
//     {
//         RR_ABORT(
//             "Exceeding buffer size! Current offset is %zu, allocation size is "
//             "%zu and total  buffer size is %zu.",
//             *DstOffset,
//             AlignedSize,
//             DstBuffer->AllocationInfo.size);
//     }
// }

Rr_AllocatedBuffer *Rr_GetCurrentAllocatedBuffer(Rr_App *App, Rr_Buffer *Buffer)
{
    size_t AllocatedBufferIndex = App->Renderer.CurrentFrameIndex % Buffer->AllocatedBufferCount;
    return &Buffer->AllocatedBuffers[AllocatedBufferIndex];
}
