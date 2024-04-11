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

SDescriptorAllocator Rr_CreateDescriptorAllocator(VkDevice Device, size_t MaxSets, SDescriptorPoolSizeRatio* Ratios, size_t RatioCount);
VkDescriptorSet Rr_AllocateDescriptorSet(SDescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout);
void Rr_ResetDescriptorAllocator(SDescriptorAllocator* DescriptorAllocator, VkDevice Device);
void Rr_DestroyDescriptorAllocator(SDescriptorAllocator* DescriptorAllocator, VkDevice Device);

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

void Rr_InitDescriptorWriter(SDescriptorWriter* Writer, size_t Images, size_t Buffers);
void Rr_WriteDescriptor_Image(SDescriptorWriter* Writer, u32 Binding, VkImageView View, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type);
void Rr_WriteDescriptor_Buffer(SDescriptorWriter* Writer, u32 Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type);
void Rr_DestroyDescriptorWriter(SDescriptorWriter* Writer);
void Rr_UpdateDescriptorSet(SDescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;

void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type);
void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder);
VkDescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags);
