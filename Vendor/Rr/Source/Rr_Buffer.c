#include "Rr_Buffer.h"

#include <stdio.h>

#include <SDL3/SDL_log.h>

#include "Rr_Vulkan.h"
#include "Rr_Renderer.h"
#include "Rr_Util.h"

Rr_Buffer Rr_CreateBuffer(const Rr_Renderer* Renderer, const size_t Size, const VkBufferUsageFlags UsageFlags, const VmaMemoryUsage MemoryUsage, const b32 bHostMapped)
{
    Rr_Buffer Buffer;

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

    vmaCreateBuffer(Renderer->Allocator, &BufferInfo, &AllocationInfo, &Buffer.Handle, &Buffer.Allocation, &Buffer.AllocationInfo);

    return Buffer;
}

Rr_Buffer Rr_CreateDeviceVertexBuffer(const Rr_Renderer* Renderer, size_t Size)
{
    Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer Rr_CreateDeviceUniformBuffer(const Rr_Renderer* Renderer, const size_t Size)
{
    // Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer Rr_CreateMappedBuffer(const Rr_Renderer* Renderer, const size_t Size, const VkBufferUsageFlags UsageFlags)
{
    return Rr_CreateBuffer(Renderer, Size, UsageFlags, VMA_MEMORY_USAGE_AUTO, true);
}

Rr_Buffer Rr_CreateMappedVertexBuffer(const Rr_Renderer* Renderer, const size_t Size)
{
    return Rr_CreateMappedBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);
}

void Rr_DestroyBuffer(const Rr_Renderer* Renderer, const Rr_Buffer* Buffer)
{
    vmaDestroyBuffer(Renderer->Allocator, Buffer->Handle, Buffer->Allocation);
}

VkDeviceAddress Rr_GetBufferAddress(const Rr_Renderer* const Renderer, const Rr_Buffer* const Buffer)
{
    const VkBufferDeviceAddressInfo DeviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = Buffer->Handle
    };
    return vkGetBufferDeviceAddress(Renderer->Device, &DeviceAddressInfo);
}

void Rr_UploadToDeviceBufferImmediate(
    const Rr_Renderer* Renderer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    const size_t Size)
{
    const VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_Buffer HostMappedBuffer = Rr_CreateMappedBuffer(
        Renderer,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    SDL_memcpy(HostMappedBuffer.AllocationInfo.pMappedData, Data, Size);
    const VkBufferCopy BufferCopy = {
        .dstOffset = 0,
        .size = Size,
        .srcOffset = 0
    };
    vkCmdCopyBuffer(
        CommandBuffer,
        HostMappedBuffer.Handle,
        DstBuffer->Handle,
        1,
        &BufferCopy);
    Rr_EndImmediate(Renderer);
    Rr_DestroyBuffer(Renderer, &HostMappedBuffer);
}

void Rr_CopyToDeviceUniformBuffer(
    const Rr_Renderer* Renderer,
    const VkCommandBuffer CommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    const size_t Size)
{
    if (StagingBuffer->CurrentOffset + Size > RR_STAGING_BUFFER_SIZE)
    {
        SDL_LogError(
            SDL_LOG_CATEGORY_VIDEO,
            "Exceeding staging buffer size! Current offset is %zu, allocation size is %zu and total staging buffer size is %zu.",
            StagingBuffer->CurrentOffset,
            Size,
            StagingBuffer->Buffer.AllocationInfo.size);
        abort();
    }
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    const size_t Offset = StagingBuffer->CurrentOffset;
    SDL_memcpy(StagingBuffer->Buffer.AllocationInfo.pMappedData + Offset, Data, Size);
    StagingBuffer->CurrentOffset = Rr_Align(Offset + Size, Alignment);

    VkBufferMemoryBarrier2 BufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
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
        StagingBuffer->Buffer.Handle,
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
    const Rr_Renderer* Renderer,
    const Rr_Buffer* DstBuffer,
    const void* Data,
    const size_t Size,
    size_t* DstOffset)
{
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    const size_t AlignedSize = Rr_Align(Size, Alignment);
    if (*DstOffset + AlignedSize < DstBuffer->AllocationInfo.size)
    {
        SDL_memcpy(DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data, Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, Alignment);
    }
}
