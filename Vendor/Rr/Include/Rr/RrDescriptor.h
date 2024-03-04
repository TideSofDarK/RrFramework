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

typedef enum EDescriptorWriterEntryType
{
    EDescriptorWriterEntryType_Buffer,
    EDescriptorWriterEntryType_Image
} EDescriptorWriterEntryType;

typedef struct SDescriptorWriterEntry
{
    EDescriptorWriterEntryType Type;
    size_t Index;
} SDescriptorWriterEntry;

typedef struct SDescriptorWriter
{
    VkDescriptorImageInfo* ImageInfos;
    VkDescriptorBufferInfo* BufferInfos;
    SDescriptorWriterEntry* Entries;
    VkWriteDescriptorSet* Writes;
} SDescriptorWriter;

void DescriptorWriter_Init(SDescriptorWriter* Writer, size_t Images, size_t Buffers);
void DescriptorWriter_WriteImage(SDescriptorWriter* Writer, u32 Binding, VkImageView View, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type);
void DescriptorWriter_WriteBuffer(SDescriptorWriter* Writer, u32 Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type);
void DescriptorWriter_Cleanup(SDescriptorWriter* Writer);
void DescriptorWriter_Update(SDescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set);
