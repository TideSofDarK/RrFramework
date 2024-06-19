#pragma once

#include "Rr/Rr_Defines.h"
#include "Rr_Memory.h"

#include <volk.h>

typedef struct Rr_DescriptorPoolSizeRatio Rr_DescriptorPoolSizeRatio;
struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    Rr_F32 Ratio;
};

typedef struct Rr_DescriptorAllocator Rr_DescriptorAllocator;
struct Rr_DescriptorAllocator
{
    Rr_Arena *Arena;
    Rr_SliceType(Rr_DescriptorPoolSizeRatio) Ratios;
    Rr_SliceType(VkDescriptorPool) FullPools;
    Rr_SliceType(VkDescriptorPool) ReadyPools;
    Rr_USize SetsPerPool;
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
    Rr_USize Index;
};

typedef struct Rr_DescriptorWriter Rr_DescriptorWriter;
struct Rr_DescriptorWriter
{
    Rr_SliceType(VkDescriptorImageInfo) ImageInfos;
    Rr_SliceType(VkDescriptorBufferInfo) BufferInfos;
    Rr_SliceType(Rr_DescriptorWriterEntry) Entries;
    Rr_SliceType(VkWriteDescriptorSet) Writes;
};

typedef enum Rr_GenericDescriptorSetLayout
{
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
} Rr_GenericDescriptorSetLayout;

extern Rr_DescriptorAllocator
Rr_CreateDescriptorAllocator(
    VkDevice Device,
    Rr_USize MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    Rr_USize RatioCount,
    Rr_Arena *Arena);

extern VkDescriptorSet
Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator *DescriptorAllocator,
    VkDevice Device,
    VkDescriptorSetLayout Layout);

extern void
Rr_ResetDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    VkDevice Device);

extern void
Rr_DestroyDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    VkDevice Device);

extern Rr_DescriptorWriter
Rr_CreateDescriptorWriter(
    Rr_USize Images,
    Rr_USize Buffers,
    struct Rr_Arena *Arena);

extern void
Rr_WriteImageDescriptor(
    Rr_DescriptorWriter *Writer,
    Rr_U32 Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void
Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter *Writer,
    Rr_U32 Binding,
    Rr_U32 Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void
Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter *Writer,
    Rr_U32 Binding,
    VkBuffer Buffer,
    Rr_USize Size,
    Rr_USize Offset,
    VkDescriptorType Type,
    struct Rr_Arena *Arena);

extern void
Rr_ResetDescriptorWriter(Rr_DescriptorWriter *Writer);

extern void
Rr_UpdateDescriptorSet(
    Rr_DescriptorWriter *Writer,
    VkDevice Device,
    VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_LAYOUT_BINDINGS];
    Rr_U32 Count;
};

extern void
Rr_AddDescriptor(
    Rr_DescriptorLayoutBuilder *Builder,
    Rr_U32 Binding,
    VkDescriptorType Type);

extern void
Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder *Builder,
    Rr_U32 Binding,
    Rr_U32 Count,
    VkDescriptorType Type);

extern void
Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder *Builder);

extern VkDescriptorSetLayout
Rr_BuildDescriptorLayout(
    Rr_DescriptorLayoutBuilder *Builder,
    VkDevice Device,
    VkShaderStageFlags ShaderStageFlags);
