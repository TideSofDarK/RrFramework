#include "RrBuffer.h"

#include "RrTypes.h"
#include "RrVulkan.h"

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
