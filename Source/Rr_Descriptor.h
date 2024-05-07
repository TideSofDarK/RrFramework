#pragma once

#include "Rr_Defines.h"
#include "Rr_Vulkan.h"

typedef struct Rr_DescriptorSetLayout
{
    VkDescriptorSetLayout Layout;
} Rr_DescriptorSetLayout;

typedef struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} Rr_DescriptorPoolSizeRatio;

typedef struct Rr_DescriptorAllocator
{
    Rr_DescriptorPoolSizeRatio* Ratios;
    VkDescriptorPool* FullPools;
    VkDescriptorPool* ReadyPools;
    size_t SetsPerPool;
} Rr_DescriptorAllocator;

typedef enum Rr_DescriptorWriterEntryType
{
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE
} Rr_DescriptorWriterEntryType;

typedef struct Rr_DescriptorWriterEntry
{
    Rr_DescriptorWriterEntryType Type;
    size_t Index;
} Rr_DescriptorWriterEntry;

typedef struct Rr_DescriptorWriter
{
    VkDescriptorImageInfo* ImageInfos;
    VkDescriptorBufferInfo* BufferInfos;
    Rr_DescriptorWriterEntry* Entries;
    VkWriteDescriptorSet* Writes;
} Rr_DescriptorWriter;

typedef enum Rr_GenericDescriptorSetLayout
{
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
} Rr_GenericDescriptorSetLayout;

Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(VkDevice Device, size_t MaxSets, Rr_DescriptorPoolSizeRatio* Ratios, size_t RatioCount);
VkDescriptorSet Rr_AllocateDescriptorSet(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout);
void Rr_ResetDescriptorAllocator(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device);
void Rr_DestroyDescriptorAllocator(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device);

Rr_DescriptorWriter Rr_CreateDescriptorWriter(size_t Images, size_t Buffers);
void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type);
void Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    u32 Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type);
void Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkBuffer Buffer,
    size_t Size,
    size_t Offset,
    VkDescriptorType Type);
void Rr_ResetDescriptorWriter(Rr_DescriptorWriter* Writer);
void Rr_DestroyDescriptorWriter(Rr_DescriptorWriter* Writer);
void Rr_UpdateDescriptorSet(Rr_DescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_LAYOUT_BINDINGS];
    u32 Count;
} Rr_DescriptorLayoutBuilder;

void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type);
void Rr_AddDescriptorArray(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, u32 Count, VkDescriptorType Type);
void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder);
VkDescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags);
