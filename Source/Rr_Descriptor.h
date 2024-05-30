#pragma once

#include "Rr_Defines.h"
#include "Rr_Memory.h"

#include <volk.h>

typedef struct Rr_DescriptorPoolSizeRatio
{
    VkDescriptorType Type;
    f32 Ratio;
} Rr_DescriptorPoolSizeRatio;

typedef struct Rr_DescriptorAllocator
{
    Rr_Arena* Arena;
    Rr_SliceType(Rr_DescriptorPoolSizeRatio) Ratios;
    Rr_SliceType(VkDescriptorPool) FullPools;
    Rr_SliceType(VkDescriptorPool) ReadyPools;
    usize SetsPerPool;
} Rr_DescriptorAllocator;

typedef enum Rr_DescriptorWriterEntryType
{
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
    RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE
} Rr_DescriptorWriterEntryType;

typedef struct Rr_DescriptorWriterEntry
{
    Rr_DescriptorWriterEntryType Type;
    usize Index;
} Rr_DescriptorWriterEntry;

typedef struct Rr_DescriptorWriter
{
    Rr_SliceType(VkDescriptorImageInfo) ImageInfos;
    Rr_SliceType(VkDescriptorBufferInfo) BufferInfos;
    Rr_SliceType(Rr_DescriptorWriterEntry) Entries;
    Rr_SliceType(VkWriteDescriptorSet) Writes;
} Rr_DescriptorWriter;

typedef enum Rr_GenericDescriptorSetLayout
{
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_GLOBALS,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_MATERIAL,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_DRAW,
    RR_GENERIC_DESCRIPTOR_SET_LAYOUT_COUNT,
} Rr_GenericDescriptorSetLayout;

extern Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(VkDevice Device, usize MaxSets, Rr_DescriptorPoolSizeRatio* Ratios, usize RatioCount, Rr_Arena* Arena);
extern VkDescriptorSet Rr_AllocateDescriptorSet(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device, VkDescriptorSetLayout Layout);
extern void Rr_ResetDescriptorAllocator(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device);
extern void Rr_DestroyDescriptorAllocator(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device);

extern Rr_DescriptorWriter Rr_CreateDescriptorWriter(usize Images, usize Buffers, struct Rr_Arena* Arena);
extern void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena* Arena);
extern void Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    u32 Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    struct Rr_Arena* Arena);
extern void Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkBuffer Buffer,
    usize Size,
    usize Offset,
    VkDescriptorType Type,
    struct Rr_Arena* Arena);
extern void Rr_ResetDescriptorWriter(Rr_DescriptorWriter* Writer);
extern void Rr_UpdateDescriptorSet(Rr_DescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_LAYOUT_BINDINGS];
    u32 Count;
} Rr_DescriptorLayoutBuilder;

extern void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type);
extern void Rr_AddDescriptorArray(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, u32 Count, VkDescriptorType Type);
extern void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder);
extern VkDescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder* Builder, VkDevice Device, VkShaderStageFlags ShaderStageFlags);
