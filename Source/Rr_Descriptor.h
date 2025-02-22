#pragma once

#include "Rr_Memory.h"
#include "Rr_Vulkan.h"

#include <Rr/Rr_Pipeline.h>

#define RR_MAX_BINDINGS 16
#define RR_MAX_SETS     4

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
    RR_SLICE(Rr_DescriptorPoolSizeRatio) Ratios;
    RR_SLICE(VkDescriptorPool) FullPools;
    RR_SLICE(VkDescriptorPool) ReadyPools;
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
    RR_SLICE(VkDescriptorImageInfo) ImageInfos;
    RR_SLICE(VkDescriptorBufferInfo) BufferInfos;
    RR_SLICE(Rr_DescriptorWriterEntry) Entries;
    RR_SLICE(VkWriteDescriptorSet) Writes;
    Rr_Arena *Arena;
};

extern Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(
    Rr_Device *Device,
    size_t MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    size_t RatioCount,
    Rr_Arena *Arena);

extern VkDescriptorSet Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device,
    VkDescriptorSetLayout Layout);

extern void Rr_ResetDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device);

extern void Rr_DestroyDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device);

extern Rr_DescriptorWriter *Rr_CreateDescriptorWriter(
    size_t SamplerCount,
    size_t ImageCount,
    size_t BufferCount,
    Rr_Arena *Arena);

extern void Rr_WriteSamplerDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkSampler Sampler);

extern void Rr_WriteSampledImageDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkImageLayout Layout);

extern void Rr_WriteCombinedImageSamplerDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout);

extern void Rr_WriteUniformBufferDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkBuffer Buffer,
    size_t Size,
    size_t Offset);

extern void Rr_WriteStorageBufferDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkBuffer Buffer,
    size_t Size,
    size_t Offset);

extern void Rr_ResetDescriptorWriter(Rr_DescriptorWriter *Writer);

extern void Rr_UpdateDescriptorSet(
    Rr_DescriptorWriter *Writer,
    Rr_Device *Device,
    VkDescriptorSet Set);

typedef struct Rr_DescriptorLayoutBuilder Rr_DescriptorLayoutBuilder;
struct Rr_DescriptorLayoutBuilder
{
    VkDescriptorSetLayoutBinding Bindings[RR_MAX_SETS];
    uint32_t Count;
};

extern void Rr_AddDescriptor(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage);

extern void Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    uint32_t Count,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage);

extern void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder *Builder);

extern VkDescriptorSetLayout Rr_BuildDescriptorLayout(
    Rr_DescriptorLayoutBuilder *Builder,
    Rr_Device *Device);

/* */

typedef struct Rr_DescriptorSetImageBinding Rr_DescriptorSetImageBinding;
struct Rr_DescriptorSetImageBinding
{
    VkImageView View;
    VkSampler Sampler;
    VkImageLayout Layout;
};

typedef struct Rr_DescriptorSetBufferBinding Rr_DescriptorSetBufferBinding;
struct Rr_DescriptorSetBufferBinding
{
    VkBuffer Handle;
    size_t Size;
    size_t Offset;
};

typedef struct Rr_DescriptorSetBinding Rr_DescriptorSetBinding;
struct Rr_DescriptorSetBinding
{
    union
    {
        Rr_DescriptorSetImageBinding Image;
        Rr_DescriptorSetBufferBinding Buffer;
        VkSampler Sampler;
    };
    Rr_PipelineBindingType Type;
};

typedef enum
{
    RR_DESCRIPTOR_SET_STATE_FLAG_DIRTY_BIT = (1 << RR_MAX_BINDINGS),
    RR_DESCRIPTOR_SET_STATE_FLAG_USED_BIT = (1 << (RR_MAX_BINDINGS + 1)),
} Rr_DescriptorSetStateFlagsBits;
typedef uint32_t Rr_DescriptorSetStateFlags;

typedef struct Rr_DescriptorSetState Rr_DescriptorSetState;
struct Rr_DescriptorSetState
{
    Rr_DescriptorSetBinding Bindings[RR_MAX_BINDINGS];
    VkDescriptorSetLayout Layout;
    Rr_DescriptorSetStateFlags Flags; /* First RR_MAX_BINDINGS bits
                                         are reserved for "used bindings"
                                         flags. */
};

typedef struct Rr_DescriptorsState Rr_DescriptorsState;
struct Rr_DescriptorsState
{
    Rr_DescriptorSetState SetStates[RR_MAX_SETS];
    bool Dirty;
};

extern void Rr_InvalidateDescriptorState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *PipelineLayout);

extern void Rr_UpdateDescriptorsState(
    Rr_DescriptorsState *State,
    size_t SetIndex,
    size_t BindingIndex,
    Rr_DescriptorSetBinding *Binding);

extern void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_PipelineLayout *PipelineLayout,
    Rr_Device *Device,
    VkCommandBuffer CommandBuffer,
    VkPipelineBindPoint PipelineBindPoint);
