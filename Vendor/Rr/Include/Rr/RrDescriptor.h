#pragma once

#include "RrVulkan.h"
#include "RrCore.h"
#include "RrArray.h"

typedef struct SDescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} SDescriptorPoolSizeRatio;

typedef struct SDescriptorAllocator
{
    SDescriptorPoolSizeRatio* Ratios;
    VkDescriptorPool* FullPools;
    VkDescriptorPool* ReadyPools;
    size_t SetsPerPool;
} SDescriptorAllocator;

void DescriptorAllocator_Init(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, size_t MaxSets, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount);
VkDescriptorSet DescriptorAllocator_Allocate(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout);
void DescriptorAllocator_ClearPools(SDescriptorAllocator* DescriptorAllocator, VkDevice Device);
void DescriptorAllocator_Cleanup(SDescriptorAllocator* DescriptorAllocator, VkDevice Device);

typedef struct SDescriptorWriter
{

} SDescriptorWriter;
