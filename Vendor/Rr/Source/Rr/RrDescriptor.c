#include "RrDescriptor.h"

#include <SDL3/SDL.h>

#include "RrVulkan.h"

VkDescriptorPool DescriptorAllocator_CreatePool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t SetCount, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    VkDescriptorPoolSize* PoolSizes = NULL;
    RrArray_Init(&PoolSizes, VkDescriptorPoolSize, RatioCount);
    for (size_t Index = 0; Index < RatioCount; Index++)
    {
        SDescriptorPoolSizeRatio* Ratio = &Ratios[Index];
        RrArray_Push((void*)&PoolSizes, &(VkDescriptorPoolSize){ .type = Ratio->Type, .descriptorCount = (u32)(Ratio->Ratio * (f32)SetCount) });
    }

    VkDescriptorPoolCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = SetCount,
        .poolSizeCount = RatioCount,
        .pPoolSizes = PoolSizes
    };

    VkDescriptorPool NewPool;
    vkCreateDescriptorPool(Device, &Info, NULL, &NewPool);
    return NewPool;
}

void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t MaxSets, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount)
{
    RrArray_Init(&DescriptorAllocator->Ratios, SDescriptorPoolSizeRatio, RatioCount);
    SDL_memcpy(DescriptorAllocator->Ratios, Ratios, RatioCount * sizeof(SDescriptorPoolSizeRatio));

    VkDescriptorPool NewPool = DescriptorAllocator_CreatePool(DescriptorAllocator, Device, MaxSets, Ratios, RatioCount);
    RrArray_Init(&DescriptorAllocator->ReadyPools, VkDescriptorPool, 1);
    RrArray_Push((void*)&DescriptorAllocator->ReadyPools, &NewPool);

    RrArray_Init(&DescriptorAllocator->FullPools, VkDescriptorPool, 1);

    DescriptorAllocator->SetsPerPool = (size_t)((f32)MaxSets * 1.5f);
}

void DescriptorAllocator_ClearPools(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    size_t Count = RrArray_Count(DescriptorAllocator->ReadyPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools[Index];
        vkResetDescriptorPool(Device, ReadyPool, 0);
    }

    Count = RrArray_Count(DescriptorAllocator->FullPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools[Index];
        vkResetDescriptorPool(Device, FullPool, 0);

        RrArray_Push((void*)&DescriptorAllocator->ReadyPools, &FullPool);
    }
    RrArray_Empty(DescriptorAllocator->FullPools, false);
}

void DescriptorAllocator_Cleanup(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    size_t Count = RrArray_Count(DescriptorAllocator->ReadyPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools[Index];
        vkDestroyDescriptorPool(Device, ReadyPool, NULL);
    }
    RrArray_Empty(DescriptorAllocator->ReadyPools, true);

    Count = RrArray_Count(DescriptorAllocator->FullPools);
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools[Index];
        vkDestroyDescriptorPool(Device, FullPool, NULL);
    }
    RrArray_Empty(DescriptorAllocator->FullPools, true);
    RrArray_Empty(DescriptorAllocator->Ratios, true);
}

VkDescriptorPool DescriptorAllocator_GetPool(SDescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VkDescriptorPool NewPool;
    size_t ReadyCount = RrArray_Count(DescriptorAllocator->ReadyPools);
    if (ReadyCount != 0)
    {
        NewPool = DescriptorAllocator->ReadyPools[ReadyCount - 1];
        RrArray_Pop(DescriptorAllocator->ReadyPools);
    }
    else
    {
        NewPool = DescriptorAllocator_CreatePool(
            DescriptorAllocator,
            Device,
            DescriptorAllocator->SetsPerPool,
            DescriptorAllocator->Ratios,
            RrArray_Count(DescriptorAllocator->Ratios));

        DescriptorAllocator->SetsPerPool = (size_t)((float)DescriptorAllocator->SetsPerPool * 1.5f);

        if (DescriptorAllocator->SetsPerPool > 4096)
        {
            DescriptorAllocator->SetsPerPool = 4096;
        }
    }

    return NewPool;
}

VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout)
{
    VkDescriptorPool Pool = DescriptorAllocator_GetPool(DescriptorAllocator, Device);

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &Layout
    };

    VkDescriptorSet DescriptorSet;
    VkResult Result = vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);

    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        RrArray_Push((void*)&DescriptorAllocator->FullPools, &Pool);

        Pool = DescriptorAllocator_GetPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        VK_ASSERT(vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet));
    }

    RrArray_Push((void*)&DescriptorAllocator->ReadyPools, &Pool);
    return DescriptorSet;
}
