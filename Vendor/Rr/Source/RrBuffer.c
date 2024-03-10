#include "RrBuffer.h"

#include "RrTypes.h"
#include "RrVulkan.h"

void AllocatedBuffer_Init(SAllocatedBuffer* Buffer, VmaAllocator Allocator, size_t Size, VkBufferUsageFlags UsageFlags, VmaMemoryUsage MemoryUsage, b32 bHostMapped)
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

void AllocatedBuffer_Cleanup(SAllocatedBuffer* Buffer, VmaAllocator Allocator)
{
    vmaDestroyBuffer(Allocator, Buffer->Handle, Buffer->Allocation);
}
