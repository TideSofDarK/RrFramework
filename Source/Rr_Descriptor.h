#pragma once

#include "Rr_Memory.h"
#include "Rr_Vulkan.h"

typedef struct Rr_DescriptorPoolSizeRatio Rr_DescriptorPoolSizeRatio;
struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    float Ratio;
};

typedef struct Rr_DescriptorAllocator Rr_DescriptorAllocator;
struct Rr_DescriptorAllocator
{
    Rr_Arena *Arena;
    RR_SLICE_TYPE(Rr_DescriptorPoolSizeRatio) Ratios;
    RR_SLICE_TYPE(VkDescriptorPool) FullPools;
    RR_SLICE_TYPE(VkDescriptorPool) ReadyPools;
    size_t SetsPerPool;
};

typedef enum Rr_DescriptorWriterEntryType
{
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE
} Rr_DescriptorWriterEntryType;

typedef struct Rr_DescriptorWriterEntry Rr_DescriptorWriterEntry;
struct Rr_DescriptorWriterEntry
{
    Rr_DescriptorWriterEntryType Type;
    size_t Index;
};

typedef struct Rr_DescriptorWriter Rr_DescriptorWriter;
struct Rr_DescriptorWriter
{
    RR_SLICE_TYPE(VkDescriptorImageInfo) ImageInfos;
    RR_SLICE_TYPE(VkDescriptorBufferInfo) BufferInfos;
    RR_SLICE_TYPE(Rr_DescriptorWriterEntry) Entries;
    RR_SLICE_TYPE(VkWriteDescriptorSet) Writes;
};

typedef enum Rr_GenericDescriptorSetLayout
{
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
} Rr_GenericDescriptorSetLayout;

extern Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(
    VkDevice Device,
    size_t MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    size_t RatioCount,
    Rr_Arena *Arena);

extern VkDescriptorSet Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator *DescriptorAllocator,
    VkDevice Device,
    VkDescriptorSetLayout Layout);

extern void Rr_ResetDescriptorAllocator(Rr_DescriptorAllocator *DescriptorAllocator, VkDevice Device);

extern void Rr_DestroyDescriptorAllocator(Rr_DescriptorAllocator *DescriptorAllocator, VkDevice Device);

extern Rr_DescriptorWriter Rr_CreateDescriptorWriter(size_t Images, size_t Buffers, struct Rr_Arena *Arena);

extern void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkBuffer Buffer,
    size_t Size,
    size_t Offset,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void Rr_ResetDescriptorWriter(Rr_DescriptorWriter *Writer);

extern void Rr_UpdateDescriptorSet(Rr_DescriptorWriter *Writer, VkDevice Device, VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_LAYOUT_BINDINGS];
    uint32_t Count;
};

extern void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder *Builder, uint32_t Binding, VkDescriptorType Type);

extern void Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    uint32_t Count,
    VkDescriptorType Type);

extern void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder *Builder);

extern VkDescriptorSetLayout Rr_BuildDescriptorLayout(
    Rr_DescriptorLayoutBuilder *Builder,
    VkDevice Device,
    VkShaderStageFlags ShaderStageFlags);
