#include "Rr_Buffer.h"

#include "Rr_Vulkan.h"
#include "Rr_Util.h"
#include "Rr_Memory.h"
#include "Rr_Renderer.h"
#include "Rr_Types.h"
#include "Rr_Object.h"

#include <SDL3/SDL_log.h>

Rr_Buffer* Rr_CreateBuffer(
    Rr_Renderer* Renderer,
    usize Size,
    VkBufferUsageFlags UsageFlags,
    VmaMemoryUsage MemoryUsage,
    bool bHostMapped)
{
    Rr_Buffer* Buffer = Rr_CreateObject(Renderer);

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

Rr_Buffer* Rr_CreateDeviceVertexBuffer(Rr_Renderer* Renderer, usize Size)
{
    Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer* Rr_CreateDeviceUniformBuffer(Rr_Renderer* Renderer, usize Size)
{
    // Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer* Rr_CreateMappedBuffer(Rr_Renderer* Renderer, usize Size, VkBufferUsageFlags UsageFlags)
{
    return Rr_CreateBuffer(Renderer, Size, UsageFlags, VMA_MEMORY_USAGE_AUTO, true);
}

Rr_Buffer* Rr_CreateMappedVertexBuffer(Rr_Renderer* Renderer, usize Size)
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

    Rr_DestroyObject(Renderer, Buffer);
}

void Rr_UploadBufferAligned(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    usize DataLength,
    usize Alignment)
{
    Rr_WriteBuffer* StagingBuffer = &UploadContext->StagingBuffer;
    if (StagingBuffer->Offset + DataLength > StagingBuffer->Buffer->AllocationInfo.size)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_VIDEO,
            "Exceeding staging buffer size! Current offset is %zu, allocation size is %zu and total staging buffer size is %zu.",
            (usize)StagingBuffer->Offset,
            (usize)DataLength,
            (usize)StagingBuffer->Buffer->AllocationInfo.size);
        abort();
    }

    VkCommandBuffer TransferCommandBuffer = UploadContext->TransferCommandBuffer;

    const usize BufferOffset = StagingBuffer->Offset;
    SDL_memcpy((char*)StagingBuffer->Buffer->AllocationInfo.pMappedData + BufferOffset, Data, DataLength);
    if (Alignment == 0)
    {
        StagingBuffer->Offset += DataLength;
    }
    else
    {
        StagingBuffer->Offset += Rr_Align(BufferOffset + DataLength, Alignment);
    }

    vkCmdPipelineBarrier(
        TransferCommandBuffer,
        SrcStageMask,
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
            .srcAccessMask = SrcAccessMask,
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

    if (!UploadContext->bUseAcquireBarriers)
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

        UploadContext->AcquireBarriers.BufferMemoryBarriers[UploadContext->AcquireBarriers.BufferMemoryBarrierCount] =
            (VkBufferMemoryBarrier){
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
        UploadContext->AcquireBarriers.BufferMemoryBarrierCount++;
    }
}

void Rr_UploadBuffer(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    VkBuffer Buffer,
    VkPipelineStageFlags SrcStageMask,
    VkAccessFlags SrcAccessMask,
    VkPipelineStageFlags DstStageMask,
    VkAccessFlags DstAccessMask,
    const void* Data,
    usize DataLength)
{
    Rr_UploadBufferAligned(Renderer, UploadContext, Buffer, SrcStageMask, SrcAccessMask, DstStageMask, DstAccessMask, Data, DataLength, 0);
}

void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize Size)
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

void Rr_UploadToUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_UploadContext* UploadContext,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize DataLength)
{
    Rr_UploadBufferAligned(
        Renderer,
        UploadContext,
        DstBuffer->Handle,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_UNIFORM_READ_BIT,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT | VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_UNIFORM_READ_BIT,
        Data,
        DataLength,
        Rr_GetUniformAlignment(Renderer));
}

void Rr_CopyToMappedUniformBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    usize Size,
    usize* DstOffset)
{
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    const usize AlignedSize = Rr_Align(Size, Alignment);
    if (*DstOffset + AlignedSize <= DstBuffer->AllocationInfo.size)
    {
        SDL_memcpy((char*)DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data, Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, Alignment);
    }
    else
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_VIDEO,
            "Exceeding buffer size! Current offset is %zu, allocation size is %zu and total  buffer size is %zu.",
            *DstOffset,
            AlignedSize,
            DstBuffer->AllocationInfo.size);
        abort();
    }
}
