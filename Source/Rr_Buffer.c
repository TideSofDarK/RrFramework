#include "Rr_Buffer.h"

#include <SDL3/SDL_log.h>

#include "Rr_Vulkan.h"
#include "Rr_Renderer.h"
#include "Rr_Util.h"
#include "Rr_Types.h"
#include "Rr_Memory.h"
#include "Rr_Array.h"

Rr_Buffer* Rr_CreateBuffer(
    Rr_Renderer* Renderer,
    size_t Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    b32 bHostMapped)
{
    Rr_Buffer* Buffer = Rr_Calloc(1, sizeof(Rr_Buffer));

    const VkBufferCreateInfo BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    };

    VmaAllocationCreateInfo AllocationInfo = {
        .usage = MemoryUsage,
    };

    if (bHostMapped)
    {
        AllocationInfo.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    }

    vmaCreateBuffer(Renderer->Allocator, &BufferInfo, &AllocationInfo, &Buffer->Handle, &Buffer->Allocation, &Buffer->AllocationInfo);

    return Buffer;
}

Rr_Buffer* Rr_CreateDeviceVertexBuffer(Rr_Renderer* Renderer, size_t Size)
{
    Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer* Rr_CreateDeviceUniformBuffer(Rr_Renderer* Renderer, size_t Size)
{
    // Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer* Rr_CreateMappedBuffer(Rr_Renderer* Renderer, size_t Size, VkBufferUsageFlags UsageFlags)
{
    return Rr_CreateBuffer(Renderer, Size, UsageFlags, VMA_MEMORY_USAGE_AUTO, true);
}

Rr_Buffer* Rr_CreateMappedVertexBuffer(Rr_Renderer* Renderer, size_t Size)
{
    return Rr_CreateMappedBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void Rr_DestroyBuffer(Rr_Renderer* Renderer, Rr_Buffer* Buffer)
{
    if (Buffer == NULL)
    {
        return;
    }

    vmaDestroyBuffer(Renderer->Allocator, Buffer->Handle, Buffer->Allocation);

    Rr_Free(Buffer);
}

void Rr_UploadBuffer(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    size_t DataLength)
{
    Rr_StagingBuffer* StagingBuffer = UploadContext->StagingBuffer;
    VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;

    const size_t BufferOffset = StagingBuffer->CurrentOffset;
    SDL_memcpy((char*)StagingBuffer->Buffer->AllocationInfo.pMappedData + BufferOffset, Data, DataLength);
    StagingBuffer->CurrentOffset += DataLength;

    vkCmdPipelineBarrier(
        TransferCommandBuffer,
        0,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        0,
        0,
        NULL,
        1,
        &(VkBufferMemoryBarrier){
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .buffer = Buffer,
            .offset = 0,
            .size = DataLength,
            .srcAccessMask = 0,
            .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
            .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        },
        0,
        NULL);

    const VkBufferCopy Copy = {
        .size = DataLength,
        .srcOffset = BufferOffset,
        .dstOffset = 0
    };

    vkCmdCopyBuffer(TransferCommandBuffer, StagingBuffer->Buffer->Handle, Buffer, 1, &Copy);

    if (UploadContext->bUnifiedQueue)
    {
        vkCmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            DstStageMask,
            0,
            0,
            NULL,
            1,
            &(VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = Buffer,
                .offset = 0,
                .size = DataLength,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = DstAccessMask,
                .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
                .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
            },
            0,
            NULL);
    }
    else
    {
        vkCmdPipelineBarrier(
            TransferCommandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
            0,
            0,
            NULL,
            1,
            &(VkBufferMemoryBarrier){
                .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
                .pNext = NULL,
                .buffer = Buffer,
                .offset = 0,
                .size = DataLength,
                .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
                .dstAccessMask = 0,
                .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
                .dstQueueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex },
            0,
            NULL);

        const VkBufferMemoryBarrier AcquireBarrier = {
            .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER,
            .pNext = NULL,
            .buffer = Buffer,
            .offset = 0,
            .size = DataLength,
            .srcAccessMask = 0,
            .dstAccessMask = DstAccessMask,
            .srcQueueFamilyIndex = Renderer->TransferQueue.FamilyIndex,
            .dstQueueFamilyIndex = Renderer->UnifiedQueue.FamilyIndex
        };
        Rr_ArrayPush(UploadContext->AcquireBarriers.BufferMemoryBarriersArray, &AcquireBarrier);
    }
}

void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size)
{
    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_Buffer* HostMappedBuffer = Rr_CreateMappedBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    SDL_memcpy(HostMappedBuffer->AllocationInfo.pMappedData, Data, Size);
    const VkBufferCopy BufferCopy = {
        .dstOffset = 0,
        .size = Size,
        .srcOffset = 0
    };
    vkCmdCopyBuffer(
        CommandBuffer,
        HostMappedBuffer->Handle,
        DstBuffer->Handle,
        1,
        &BufferCopy);
    Rr_EndImmediate(Renderer);
    Rr_DestroyBuffer(Renderer, HostMappedBuffer);
}

void Rr_CopyToDeviceUniformBuffer(
    Rr_Renderer* Renderer,
    VkCommandBuffer CommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size)
{
    if (StagingBuffer->CurrentOffset + Size > RR_STAGING_BUFFER_SIZE)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_VIDEO,
            "Exceeding staging buffer size! Current offset is %zu, allocation size is %zu and total staging buffer size is %zu.",
            StagingBuffer->CurrentOffset,
            Size,
            StagingBuffer->Buffer->AllocationInfo.size);
        abort();
    }
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    const size_t Offset = StagingBuffer->CurrentOffset;
    SDL_memcpy((char*)StagingBuffer->Buffer->AllocationInfo.pMappedData + Offset, Data, Size);
    StagingBuffer->CurrentOffset = Rr_Align(Offset + Size, Alignment);

    VkBufferMemoryBarrier2 BufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .srcAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .size = Size,
        .offset = 0,
        .buffer = DstBuffer->Handle,
    };

    const VkDependencyInfo DependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &BufferBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);

    const VkBufferCopy BufferCopy = {
        .dstOffset = 0,
        .size = Size,
        .srcOffset = Offset
    };

    vkCmdCopyBuffer(
        CommandBuffer,
        StagingBuffer->Buffer->Handle,
        DstBuffer->Handle,
        1,
        &BufferCopy);

    BufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
    BufferBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
    BufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
    BufferBarrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT;

    vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);
}

void Rr_CopyToMappedUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size,
    size_t* DstOffset)
{
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    const size_t AlignedSize = Rr_Align(Size, Alignment);
    if (*DstOffset + AlignedSize < DstBuffer->AllocationInfo.size)
    {
        SDL_memcpy((char*)DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data, Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, Alignment);
    }
}

Rr_StagingBuffer* Rr_CreateStagingBuffer(Rr_Renderer* Renderer, size_t Size)
{
    Rr_StagingBuffer* StagingBuffer = Rr_Calloc(1, sizeof(Rr_StagingBuffer));
    *StagingBuffer = (Rr_StagingBuffer){
        .CurrentOffset = 0,
        .Buffer = Rr_CreateMappedBuffer(Renderer, Size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT)
    };
    return StagingBuffer;
}

void Rr_DestroyStagingBuffer(Rr_Renderer* Renderer, Rr_StagingBuffer* StagingBuffer)
{
    Rr_DestroyBuffer(Renderer, StagingBuffer->Buffer);
}
