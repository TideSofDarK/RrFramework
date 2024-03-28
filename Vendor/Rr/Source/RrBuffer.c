#include "RrBuffer.h"

#include "RrTypes.h"
#include "RrVulkan.h"

void Rr_InitBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped)
{
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

    VK_ASSERT(vmaCreateBuffer(Allocator, &BufferInfo, &AllocationInfo, &Buffer->Handle, &Buffer->Allocation, &Buffer->AllocationInfo))
}

void Rr_InitMappedBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags)
{
    Rr_InitBuffer(Buffer, Allocator, Size, UsageFlags, VMA_MEMORY_USAGE_AUTO_PREFER_HOST, true);
}

void Rr_DestroyBuffer(Rr_Buffer* Buffer, VmaAllocator Allocator)
{
    vmaDestroyBuffer(Allocator, Buffer->Handle, Buffer->Allocation);
}
