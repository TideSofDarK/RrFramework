#include "RrBuffer.h"

#include <stdio.h>

#include "RrTypes.h"
#include "RrVulkan.h"
#include "RrRenderer.h"
#include "RrLib.h"
#include "RrUtil.h"

Rr_Buffer Rr_CreateBuffer(VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped)
{
    Rr_Buffer Buffer;

    VkBufferCreateInfo BufferInfo = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .pNext = NULL,
        .size = Size,
        .usage = UsageFlags,
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

Rr_Buffer Rr_CopyBufferToHost(Rr_Renderer* const Renderer, Rr_Buffer* Buffer)
{
    VmaAllocationInfo AllocationInfo;
    vmaGetAllocationInfo(Renderer->Allocator, Buffer->Allocation, &AllocationInfo);

    Rr_Buffer HostMappedBuffer = Rr_CreateMappedBuffer(Renderer->Allocator, AllocationInfo.size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    Rr_BeginImmediate(Renderer);
    //    Rr_ImageBarrier SrcTransition = {
    //        .Image = Image->Handle,
    //        .Layout = VK_IMAGE_LAYOUT_UNDEFINED,
    //        .StageMask = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
    //        .AccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
    //        .CommandBuffer = Renderer->ImmediateMode.CommandBuffer,
    //    };
    //    Rr_ChainImageBarrier_Aspect(&SrcTransition, VK_PIPELINE_STAGE_2_COPY_BIT, VK_ACCESS_2_TRANSFER_READ_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
    //
    VkBufferCopy Copy = {
        .dstOffset = 0,
        .size = AllocationInfo.size,
        .srcOffset = 0
    };

    vkCmdCopyBuffer(
        Renderer->ImmediateMode.CommandBuffer,
        Buffer->Handle,
        HostMappedBuffer.Handle,
        1,
        &Copy);

    Rr_EndImmediate(Renderer);

    for (int Index = 0; Index < 960; ++Index)
    {
        fprintf(stderr, "%f\n", ((float*)HostMappedBuffer.AllocationInfo.pMappedData)[Index]);
    }

    //    Rr_SaveDepthToFile(Renderer, &Buffer, Image->Extent.width, Image->Extent.height);

    Rr_DestroyBuffer(&HostMappedBuffer, Renderer->Allocator);
}

void Rr_UploadToDeviceBuffer(
    Rr_Renderer* Renderer,
    VkCommandBuffer CommandBuffer,
    Rr_StagingBuffer* StagingBuffer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size)
{
    if (StagingBuffer->CurrentOffset + Size > RR_STAGING_BUFFER_SIZE)
    {
        return;
    }
    size_t Offset = StagingBuffer->CurrentOffset;
    SDL_memcpy(StagingBuffer->Buffer.AllocationInfo.pMappedData + Offset, Data, Size);
    StagingBuffer->CurrentOffset = Rr_Align(Offset + Size, RR_MIN_BUFFER_ALIGNMENT);

    VkBufferMemoryBarrier2 BufferBarrier = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2,
        .pNext = NULL,
        .srcStageMask = VK_PIPELINE_STAGE_2_NONE,
        .srcAccessMask = VK_ACCESS_2_NONE,
        .dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .size = VK_WHOLE_SIZE,
        .offset = 0,
        .buffer = DstBuffer->Handle,
    };

    VkDependencyInfo DepInfo = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .pNext = NULL,
        .bufferMemoryBarrierCount = 1,
        .pBufferMemoryBarriers = &BufferBarrier,
    };

    vkCmdPipelineBarrier2(CommandBuffer, &DepInfo);

    VkBufferCopy Copy = {
        .dstOffset = 0,
        .size = Size,
        .srcOffset = Offset
    };

    vkCmdCopyBuffer(
        CommandBuffer,
        StagingBuffer->Buffer.Handle,
        DstBuffer->Handle,
        1,
        &Copy);

    BufferBarrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
    BufferBarrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
    BufferBarrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT,
    BufferBarrier.dstAccessMask = VK_ACCESS_2_UNIFORM_READ_BIT,

    vkCmdPipelineBarrier2(CommandBuffer, &DepInfo);
}

void Rr_UploadToMappedBuffer(
    Rr_Renderer* Renderer,
    Rr_Buffer* DstBuffer,
    const void* Data,
    size_t Size,
    size_t* DstOffset)
{
    const size_t AlignedSize = Rr_Align(Size, RR_MIN_BUFFER_ALIGNMENT);
    if (*DstOffset + AlignedSize < DstBuffer->AllocationInfo.size)
    {
        SDL_memcpy(DstBuffer->AllocationInfo.pMappedData + *DstOffset, Data, Size);
        *DstOffset += Size;
        *DstOffset = Rr_Align(*DstOffset, RR_MIN_BUFFER_ALIGNMENT);
    }
}
