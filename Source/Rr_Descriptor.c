#include "Rr_Descriptor.h"

#include "Rr_Log.h"
#include "Rr_Memory.h"
#include "Rr_Pipeline.h"

#include <string.h>

static VkDescriptorType Rr_GetVulkanDescriptorType(Rr_PipelineBindingType Type)
{
    switch(Type)
    {
        case RR_PIPELINE_BINDING_TYPE_COMBINED_SAMPLER:
            return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        case RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER:
            return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        case RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER:
            return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        default:
            return VK_DESCRIPTOR_TYPE_MAX_ENUM;
    }
}

static VkDescriptorPool Rr_CreateDescriptorPool(
    VkDevice Device,
    size_t SetCount,
    Rr_DescriptorPoolSizeRatio *Ratios,
    size_t RatioCount)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkDescriptorPoolSize *PoolSizes = RR_ALLOC_STRUCT_COUNT(Scratch.Arena, VkDescriptorPoolSize, RatioCount);
    for(size_t Index = 0; Index < RatioCount; Index++)
    {
        Rr_DescriptorPoolSizeRatio *Ratio = &Ratios[Index];
        PoolSizes[Index] = (VkDescriptorPoolSize){
            .type = Ratio->Type,
            .descriptorCount = (uint32_t)(Ratio->Ratio * (float)SetCount),
        };
    }

    VkDescriptorPoolCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = SetCount,
        .poolSizeCount = RatioCount,
        .pPoolSizes = PoolSizes,
    };

    VkDescriptorPool NewPool;
    vkCreateDescriptorPool(Device, &Info, NULL, &NewPool);

    Rr_DestroyScratch(Scratch);

    return NewPool;
}

Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(
    VkDevice Device,
    size_t MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    size_t RatioCount,
    Rr_Arena *Arena)
{
    Rr_DescriptorAllocator DescriptorAllocator = { 0 };
    DescriptorAllocator.Arena = Arena;
    RR_RESERVE_SLICE(&DescriptorAllocator.Ratios, RatioCount, Arena);
    memcpy(DescriptorAllocator.Ratios.Data, Ratios, RatioCount * sizeof(Rr_DescriptorPoolSizeRatio));

    VkDescriptorPool NewPool = Rr_CreateDescriptorPool(Device, MaxSets, Ratios, RatioCount);
    RR_RESERVE_SLICE(&DescriptorAllocator.ReadyPools, 1, Arena); /* NOLINT */
    *RR_PUSH_SLICE(&DescriptorAllocator.ReadyPools, Arena) = NewPool;

    RR_RESERVE_SLICE(&DescriptorAllocator.FullPools, 1, Arena); /* NOLINT */

    DescriptorAllocator.SetsPerPool = (size_t)((float)MaxSets * 1.5f);

    return DescriptorAllocator;
}

void Rr_ResetDescriptorAllocator(Rr_DescriptorAllocator *DescriptorAllocator, VkDevice Device)
{
    size_t Count = DescriptorAllocator->ReadyPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools.Data[Index];
        vkResetDescriptorPool(Device, ReadyPool, 0);
    }

    Count = DescriptorAllocator->FullPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        vkResetDescriptorPool(Device, FullPool, 0);

        *RR_PUSH_SLICE(&DescriptorAllocator->ReadyPools, NULL) = FullPool;
    }
    RR_EMPTY_SLICE(&DescriptorAllocator->FullPools);
}

void Rr_DestroyDescriptorAllocator(Rr_DescriptorAllocator *DescriptorAllocator, VkDevice Device)
{
    size_t Count = DescriptorAllocator->ReadyPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools.Data[Index];
        vkDestroyDescriptorPool(Device, ReadyPool, NULL);
    }
    RR_EMPTY_SLICE(&DescriptorAllocator->ReadyPools);

    Count = DescriptorAllocator->FullPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        vkDestroyDescriptorPool(Device, FullPool, NULL);
    }
}

VkDescriptorPool Rr_GetDescriptorPool(Rr_DescriptorAllocator *DescriptorAllocator, VkDevice Device)
{
    VkDescriptorPool NewPool;
    size_t ReadyCount = DescriptorAllocator->ReadyPools.Count;
    if(ReadyCount != 0)
    {
        NewPool = DescriptorAllocator->ReadyPools.Data[ReadyCount - 1];
        RR_POP_SLICE(&DescriptorAllocator->ReadyPools);
    }
    else
    {
        NewPool = Rr_CreateDescriptorPool(
            Device,
            DescriptorAllocator->SetsPerPool,
            DescriptorAllocator->Ratios.Data,
            DescriptorAllocator->Ratios.Count);

        DescriptorAllocator->SetsPerPool = (size_t)((float)DescriptorAllocator->SetsPerPool * 1.5f);

        if(DescriptorAllocator->SetsPerPool > 4096)
        {
            DescriptorAllocator->SetsPerPool = 4096;
        }
    }

    return NewPool;
}

VkDescriptorSet Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator *DescriptorAllocator,
    VkDevice Device,
    VkDescriptorSetLayout Layout)
{
    VkDescriptorPool Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &Layout,
    };

    VkDescriptorSet DescriptorSet;
    VkResult Result = vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);

    if(Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        *RR_PUSH_SLICE(&DescriptorAllocator->FullPools, DescriptorAllocator->Arena) = Pool;

        Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);
    }

    *RR_PUSH_SLICE(&DescriptorAllocator->ReadyPools, DescriptorAllocator->Arena) = Pool;
    return DescriptorSet;
}

Rr_DescriptorWriter Rr_CreateDescriptorWriter(size_t Images, size_t Buffers, Rr_Arena *Arena)
{
    Rr_DescriptorWriter Writer = { 0 };
    RR_RESERVE_SLICE(&Writer.ImageInfos, Images, Arena);
    RR_RESERVE_SLICE(&Writer.BufferInfos, Buffers, Arena);
    RR_RESERVE_SLICE(&Writer.Writes, Images + Buffers, Arena);
    RR_RESERVE_SLICE(&Writer.Entries, Images + Buffers, Arena);
    return Writer;
}

void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    Rr_Arena *Arena)
{
    Rr_WriteImageDescriptorAt(Writer, Binding, 0, View, Sampler, Layout, Type, Arena);
}

void Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    Rr_Arena *Arena)
{
    *RR_PUSH_SLICE(&Writer->ImageInfos, Arena) = (VkDescriptorImageInfo){
        .sampler = Sampler,
        .imageView = View,
        .imageLayout = Layout,
    };

    *RR_PUSH_SLICE(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .dstSet = VK_NULL_HANDLE,
        .descriptorCount = 1,
        .descriptorType = Type,
        .dstArrayElement = Index,
    };

    *RR_PUSH_SLICE(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
        .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE,
        .Index = Writer->ImageInfos.Count - 1,
    };
}

void Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    VkBuffer Buffer,
    size_t Size,
    size_t Offset,
    VkDescriptorType Type,
    Rr_Arena *Arena)
{
    *RR_PUSH_SLICE(&Writer->BufferInfos, Arena) = (VkDescriptorBufferInfo){
        .range = Size,
        .buffer = Buffer,
        .offset = Offset,
    };

    *RR_PUSH_SLICE(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .dstSet = VK_NULL_HANDLE,
        .descriptorCount = 1,
        .descriptorType = Type,
    };

    *RR_PUSH_SLICE(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
        .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
        .Index = Writer->BufferInfos.Count - 1,
    };
}

void Rr_ResetDescriptorWriter(Rr_DescriptorWriter *Writer)
{
    RR_EMPTY_SLICE(&Writer->ImageInfos);
    RR_EMPTY_SLICE(&Writer->BufferInfos);
    RR_EMPTY_SLICE(&Writer->Writes);
    RR_EMPTY_SLICE(&Writer->Entries);
}

void Rr_UpdateDescriptorSet(Rr_DescriptorWriter *Writer, VkDevice Device, VkDescriptorSet Set)
{
    size_t WritesCount = Writer->Writes.Count;
    for(size_t Index = 0; Index < WritesCount; ++Index)
    {
        Rr_DescriptorWriterEntry *Entry = &Writer->Entries.Data[Index];
        VkWriteDescriptorSet *Write = &Writer->Writes.Data[Index];
        Write->dstSet = Set;
        switch(Entry->Type)
        {
            case RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER:
            {
                Write->pBufferInfo = &Writer->BufferInfos.Data[Entry->Index];
            }
            break;
            case RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE:
            {
                Write->pImageInfo = &Writer->ImageInfos.Data[Entry->Index];
            }
            break;
            default:
            {
            }
            break;
        }
    }

    vkUpdateDescriptorSets(Device, (uint32_t)WritesCount, Writer->Writes.Data, 0, NULL);
}

void Rr_AddDescriptor(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage)
{
    if(Builder->Count >= RR_MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] =
        (VkDescriptorSetLayoutBinding){ .binding = Binding,
                                        .descriptorType = Rr_GetVulkanDescriptorType(Type),
                                        .descriptorCount = 1,
                                        .stageFlags = Rr_GetVulkanShaderStageFlags(ShaderStage) };
    Builder->Count++;
}

void Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    uint32_t Count,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage)
{
    if(Builder->Count >= RR_MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] =
        (VkDescriptorSetLayoutBinding){ .binding = Binding,
                                        .descriptorType = Rr_GetVulkanDescriptorType(Type),
                                        .descriptorCount = Count,
                                        .stageFlags = Rr_GetVulkanShaderStageFlags(ShaderStage) };
    Builder->Count++;
}

void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder *Builder)
{
    *Builder = (Rr_DescriptorLayoutBuilder){ 0 };
}

VkDescriptorSetLayout Rr_BuildDescriptorLayout(Rr_DescriptorLayoutBuilder *Builder, VkDevice Device)
{
    VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout DescriptorSetLayout;
    vkCreateDescriptorSetLayout(Device, &Info, NULL, &DescriptorSetLayout);

    return DescriptorSetLayout;
}

void Rr_UpdateDescriptorsState(
    Rr_DescriptorsState *State,
    size_t SetIndex,
    size_t BindingIndex,
    Rr_DescriptorSetBinding *Binding)
{
    memcpy(&State->States[SetIndex].Bindings[BindingIndex], Binding, sizeof(Rr_DescriptorSetBinding));
    State->States[SetIndex].Dirty = true;
    State->Dirty = true;
}

void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_PipelineLayout *PipelineLayout,
    VkDevice Device,
    VkCommandBuffer CommandBuffer,
    VkPipelineBindPoint PipelineBindPoint)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    if(State->Dirty != true)
    {
        return;
    }

    RR_SLICE(uint32_t) DynamicOffsets = { 0 };
    RR_RESERVE_SLICE(&DynamicOffsets, RR_MAX_BINDINGS, Scratch.Arena);

    uint32_t FirstSet = 0;
    uint32_t DescriptorSetCount = 0;
    VkDescriptorSet DescriptorSets[RR_MAX_SETS];

    Rr_DescriptorWriter Writer = { 0 };

    for(size_t SetIndex = 0; SetIndex < RR_MAX_SETS; ++SetIndex)
    {
        Rr_DescriptorSetState *SetState = State->States + SetIndex;
        if(SetState->Dirty == false)
        {
            FirstSet++;
            continue;
        }

        VkDescriptorSet DescriptorSet =
            Rr_AllocateDescriptorSet(DescriptorAllocator, Device, PipelineLayout->DescriptorSetLayouts[SetIndex]);

        for(size_t BindingIndex = 0; BindingIndex < RR_MAX_BINDINGS; ++BindingIndex)
        {
            Rr_DescriptorSetBinding *Binding = SetState->Bindings + BindingIndex;
            if(Binding->Used != true)
            {
                continue;
            }

            switch(Binding->Type)
            {
                case RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER:
                {
                    Rr_WriteBufferDescriptor(
                        &Writer,
                        BindingIndex,
                        Binding->Buffer.Handle,
                        Binding->Buffer.Size,
                        0, /* We rely on dynamic offsets! */
                        Binding->DescriptorType,
                        Scratch.Arena);

                    *RR_PUSH_SLICE(&DynamicOffsets, NULL) = Binding->Buffer.Offset;
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_COMBINED_SAMPLER:
                {
                    Rr_WriteImageDescriptor(
                        &Writer,
                        BindingIndex,
                        Binding->Image.View,
                        Binding->Image.Sampler,
                        Binding->Image.Layout,
                        Binding->DescriptorType,
                        Scratch.Arena);
                }
                break;
                default:
                {
                    Rr_LogAbort("Not implemented!");
                }
                break;
            }
        }

        Rr_UpdateDescriptorSet(&Writer, Device, DescriptorSet);
        Rr_ResetDescriptorWriter(&Writer);
    }

    if(DescriptorSetCount == 0)
    {
        return;
    }

    vkCmdBindDescriptorSets(
        CommandBuffer,
        PipelineBindPoint,
        PipelineLayout->Handle,
        FirstSet,
        DescriptorSetCount,
        DescriptorSets,
        DynamicOffsets.Count,
        DynamicOffsets.Data);

    Rr_DestroyScratch(Scratch);
}
