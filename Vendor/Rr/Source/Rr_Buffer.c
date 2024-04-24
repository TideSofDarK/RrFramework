#include "Rr_Buffer.h"

#include <stdio.h>

#include <SDL3/SDL_log.h>

#include "Rr_Vulkan.h"
#include "Rr_Renderer.h"
#include "Rr_Util.h"

Rr_Buffer Rr_CreateBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped)
{
    Rr_Buffer Buffer;

    VkBufferCreateInfo BufferInfo = {
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

    vmaCreateBuffer(Allocator, &BufferInfo, &AllocationInfo, &Buffer.Handle, &Buffer.Allocation, &Buffer.AllocationInfo);

    return Buffer;
}

Rr_Buffer Rr_CreateDeviceVertexBuffer(Rr_Renderer* Renderer, size_t Size)
{
    Size = SDL_max(Size, 64);
    return Rr_CreateBuffer(
        Renderer->Allocator,
        Size,
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer Rr_CreateDeviceUniformBuffer(Rr_Renderer* Renderer, size_t Size)
{
    // Size = SDL_max(Size, 128);
    return Rr_CreateBuffer(
        Renderer->Allocator,
        Size,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        false);
}

Rr_Buffer Rr_CreateMappedBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags)
{
    return Rr_CreateBuffer(Allocator, Size, UsageFlags, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);
}

void Rr_DestroyBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator)
{
    vmaDestroyBuffer(Allocator, Buffer->Handle, Buffer->Allocation);
}

VkDeviceAddress Rr_GetBufferAddress(Rr_Renderer* const Renderer, Rr_Buffer* const Buffer)
{
    VkBufferDeviceAddressInfo DeviceAddressInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO,
        .buffer = Buffer->Handle
    };
    return vkGetBufferDeviceAddress(Renderer->Device, &DeviceAddressInfo);
}

void Rr_UploadToDeviceBufferImmediate(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size)
{
    VkCommandBuffer CommandBuffer = Rr_BeginImmediate(Renderer);
    Rr_Buffer HostMappedBuffer = Rr_CreateMappedBuffer(
        Renderer->Allocator,
        Size,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
    SDL_memcpy(HostMappedBuffer.AllocationInfo.pMappedData, Data, Size);
    VkBufferCopy BufferCopy = {
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
    Rr_DestroyBuffer(&HostMappedBuffer, Renderer->Allocator);
}

void Rr_UploadToDeviceUniformBuffer(
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
            StagingBuffer->Buffer.AllocationInfo.size);
        abort();
    }
    const u32 Alignment = Renderer->PhysicalDevice.Properties.properties.limits.minUniformBufferOffsetAlignment;
    size_t Offset = StagingBuffer->CurrentOffset;
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

    VkDependencyInfo DependencyInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &BufferBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DependencyInfo);

    VkBufferCopy BufferCopy = {
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

void Rr_CopyToMappedBuffer(
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
        SDL_memcpy(DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data, Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, Alignment);
    }
}
