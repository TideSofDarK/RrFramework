#pragma once

#include "RrVulkan.h"
#include "RrCore.h"
#include "RrArray.h"

typedef struct Rr_Renderer Rr_Renderer;

typedef struct Rr_DescriptorSetLayout
{
    VkDescriptorSetLayout Layout;
} Rr_DescriptorSetLayout;

typedef struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} Rr_DescriptorPoolSizeRatio;

typedef struct SDescriptorAllocator
{
    Rr_DescriptorPoolSizeRatio* Ratios;
    VkDescriptorPool* FullPools;
    VkDescriptorPool* ReadyPools;
    size_t SetsPerPool;
} SDescriptorAllocator;

SDescriptorAllocator Rr_CreateDescriptorAllocator(VkDevice Device, size_t MaxSets, Rr_DescriptorPoolSizeRatio* Ratios, size_t RatioCount);
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
} Rr_DescriptorWriterEntry;

typedef struct Rr_DescriptorWriter
{
    VkDescriptorImageInfo* ImageInfos;
    VkDescriptorBufferInfo* BufferInfos;
    Rr_DescriptorWriterEntry* Entries;
    VkWriteDescriptorSet* Writes;
} Rr_DescriptorWriter;

Rr_DescriptorWriter Rr_CreateDescriptorWriter(size_t Images, size_t Buffers);
void Rr_WriteImageDescriptor(Rr_DescriptorWriter* Writer, u32 Binding, VkImageView View, VkSampler Sampler, VkImageLayout Layout, VkDescriptorType Type);
void Rr_WriteBufferDescriptor(Rr_DescriptorWriter* Writer, u32 Binding, VkBuffer Buffer, size_t Size, size_t Offset, VkDescriptorType Type);
void Rr_ResetDescriptorWriter(Rr_DescriptorWriter* Writer);
void Rr_DestroyDescriptorWriter(Rr_DescriptorWriter* Writer);
void Rr_UpdateDescriptorSet(Rr_DescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;

void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type);
void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder);
Rr_DescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags);
void Rr_DestroyDescriptorSetLayout(Rr_Renderer* Renderer, Rr_DescriptorSetLayout* DescriptorSetLayout);
