/*
 * MIT License
 *
 * Copyright (c) 2024-2025 Alexandr Semenov
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "Rr_Descriptor.h"

#include "Rr_Log.h"
#include "Rr_Pipeline.h"

#include <string.h>

static VkDescriptorPool Rr_CreateDescriptorPool(
    Rr_Device *Device,
    uint32_t SetCount,
    Rr_DescriptorPoolSizeRatio *Ratios,
    uint32_t RatioCount)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkDescriptorPoolSize *PoolSizes =
        RR_ALLOC_TYPE_COUNT(Scratch.Arena, VkDescriptorPoolSize, RatioCount);
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
    Device->CreateDescriptorPool(Device->Handle, &Info, NULL, &NewPool);

    Rr_DestroyScratch(Scratch);

    return NewPool;
}

Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(
    Rr_Device *Device,
    uint32_t MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    uint32_t RatioCount,
    Rr_Arena *Arena)
{
    Rr_DescriptorAllocator DescriptorAllocator = { 0 };
    DescriptorAllocator.Arena = Arena;
    RR_RESERVE_ARRAY(&DescriptorAllocator.Ratios, RatioCount, Arena);
    memcpy(
        DescriptorAllocator.Ratios.Data,
        Ratios,
        RatioCount * sizeof(Rr_DescriptorPoolSizeRatio));

    VkDescriptorPool NewPool =
        Rr_CreateDescriptorPool(Device, MaxSets, Ratios, RatioCount);
    RR_RESERVE_ARRAY(&DescriptorAllocator.ReadyPools, 1, Arena);
    *RR_PUSH_INTO_ARRAY(&DescriptorAllocator.ReadyPools, Arena) = NewPool;

    RR_RESERVE_ARRAY(&DescriptorAllocator.FullPools, 1, Arena);

    DescriptorAllocator.SetsPerPool = (size_t)((float)MaxSets * 1.5f);

    return DescriptorAllocator;
}

void Rr_ResetDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device)
{
    size_t Count = DescriptorAllocator->ReadyPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool =
            DescriptorAllocator->ReadyPools.Data[Index];
        Device->ResetDescriptorPool(Device->Handle, ReadyPool, 0);
    }

    Count = DescriptorAllocator->FullPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        Device->ResetDescriptorPool(Device->Handle, FullPool, 0);

        *RR_PUSH_INTO_ARRAY(&DescriptorAllocator->ReadyPools, NULL) = FullPool;
    }
    RR_CLEAR_ARRAY(&DescriptorAllocator->FullPools);
}

void Rr_DestroyDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device)
{
    size_t Count = DescriptorAllocator->ReadyPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool =
            DescriptorAllocator->ReadyPools.Data[Index];
        Device->DestroyDescriptorPool(Device->Handle, ReadyPool, NULL);
    }
    RR_CLEAR_ARRAY(&DescriptorAllocator->ReadyPools);

    Count = DescriptorAllocator->FullPools.Count;
    for(size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        Device->DestroyDescriptorPool(Device->Handle, FullPool, NULL);
    }
}

VkDescriptorPool Rr_GetDescriptorPool(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device)
{
    VkDescriptorPool NewPool;
    size_t ReadyCount = DescriptorAllocator->ReadyPools.Count;
    if(ReadyCount != 0)
    {
        NewPool = DescriptorAllocator->ReadyPools.Data[ReadyCount - 1];
        (void)RR_POP_FROM_ARRAY(&DescriptorAllocator->ReadyPools);
    }
    else
    {
        NewPool = Rr_CreateDescriptorPool(
            Device,
            DescriptorAllocator->SetsPerPool,
            DescriptorAllocator->Ratios.Data,
            (uint32_t)DescriptorAllocator->Ratios.Count);

        DescriptorAllocator->SetsPerPool =
            (size_t)((float)DescriptorAllocator->SetsPerPool * 1.5f);

        if(DescriptorAllocator->SetsPerPool > 4096)
        {
            DescriptorAllocator->SetsPerPool = 4096;
        }
    }

    return NewPool;
}

VkDescriptorSet Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device,
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
    VkResult Result = Device->AllocateDescriptorSets(
        Device->Handle,
        &AllocateInfo,
        &DescriptorSet);

    if(Result == VK_ERROR_OUT_OF_POOL_MEMORY ||
       Result == VK_ERROR_FRAGMENTED_POOL)
    {
        *RR_PUSH_INTO_ARRAY(
            &DescriptorAllocator->FullPools,
            DescriptorAllocator->Arena) = Pool;

        Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        Device->AllocateDescriptorSets(
            Device->Handle,
            &AllocateInfo,
            &DescriptorSet);
    }

    *RR_PUSH_INTO_ARRAY(
        &DescriptorAllocator->ReadyPools,
        DescriptorAllocator->Arena) = Pool;
    return DescriptorSet;
}

Rr_DescriptorWriter *Rr_CreateDescriptorWriter(
    size_t SamplerCount,
    size_t ImageCount,
    size_t BufferCount,
    Rr_Arena *Arena)
{
    Rr_DescriptorWriter *Writer = RR_ALLOC_TYPE(Arena, Rr_DescriptorWriter);
    Writer->Arena = Arena;
    RR_RESERVE_ARRAY(&Writer->ImageInfos, ImageCount, Arena);
    RR_RESERVE_ARRAY(&Writer->BufferInfos, BufferCount, Arena);
    RR_RESERVE_ARRAY(&Writer->Writes, ImageCount + BufferCount, Arena);
    RR_RESERVE_ARRAY(&Writer->Entries, ImageCount + BufferCount, Arena);
    return Writer;
}

void Rr_WriteSamplerDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkSampler Sampler)
{
    Rr_Arena *Arena = Writer->Arena;

    *RR_PUSH_INTO_ARRAY(&Writer->ImageInfos, Arena) = (VkDescriptorImageInfo){
        .sampler = Sampler,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .dstArrayElement = Index,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
        .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE,
        .Index = Writer->ImageInfos.Count - 1,
    };
}

void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkImageLayout Layout,
    VkDescriptorType Type)
{
    Rr_Arena *Arena = Writer->Arena;

    *RR_PUSH_INTO_ARRAY(&Writer->ImageInfos, Arena) = (VkDescriptorImageInfo){
        .imageView = View,
        .imageLayout = Layout,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .descriptorCount = 1,
        .descriptorType = Type,
        .dstArrayElement = Index,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
        .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE,
        .Index = Writer->ImageInfos.Count - 1,
    };
}

void Rr_WriteCombinedImageSamplerDescriptor(
    Rr_DescriptorWriter *Writer,
    uint32_t Binding,
    uint32_t Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout)
{
    Rr_Arena *Arena = Writer->Arena;

    *RR_PUSH_INTO_ARRAY(&Writer->ImageInfos, Arena) = (VkDescriptorImageInfo){
        .sampler = Sampler,
        .imageView = View,
        .imageLayout = Layout,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .dstArrayElement = Index,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
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
    *RR_PUSH_INTO_ARRAY(&Writer->BufferInfos, Arena) = (VkDescriptorBufferInfo){
        .range = Size,
        .buffer = Buffer,
        .offset = Offset,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Writes, Arena) = (VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .descriptorCount = 1,
        .descriptorType = Type,
    };

    *RR_PUSH_INTO_ARRAY(&Writer->Entries, Arena) = (Rr_DescriptorWriterEntry){
        .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER,
        .Index = Writer->BufferInfos.Count - 1,
    };
}

void Rr_ResetDescriptorWriter(Rr_DescriptorWriter *Writer)
{
    RR_CLEAR_ARRAY(&Writer->ImageInfos);
    RR_CLEAR_ARRAY(&Writer->BufferInfos);
    RR_CLEAR_ARRAY(&Writer->Writes);
    RR_CLEAR_ARRAY(&Writer->Entries);
}

void Rr_UpdateDescriptorSet(
    Rr_DescriptorWriter *Writer,
    Rr_Device *Device,
    VkDescriptorSet Set)
{
    size_t WritesCount = Writer->Writes.Count;
    if(WritesCount == 0)
    {
        return;
    }
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

    Device->UpdateDescriptorSets(
        Device->Handle,
        (uint32_t)WritesCount,
        Writer->Writes.Data,
        0,
        NULL);
}

void Rr_AddDescriptor(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage)
{
    if(Builder->Count >= RR_MAX_SETS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Rr_ToVulkanDescriptorType(Type),
        .descriptorCount = 1,
        .stageFlags = Rr_ToVulkanShaderStageFlags(ShaderStage)
    };
    Builder->Count++;
}

void Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder *Builder,
    uint32_t Binding,
    uint32_t Count,
    Rr_PipelineBindingType Type,
    Rr_ShaderStage ShaderStage)
{
    if(Builder->Count >= RR_MAX_SETS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Rr_ToVulkanDescriptorType(Type),
        .descriptorCount = Count,
        .stageFlags = Rr_ToVulkanShaderStageFlags(ShaderStage)
    };
    Builder->Count++;
}

void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder *Builder)
{
    *Builder = (Rr_DescriptorLayoutBuilder){ 0 };
}

VkDescriptorSetLayout Rr_BuildDescriptorLayout(
    Rr_DescriptorLayoutBuilder *Builder,
    Rr_Device *Device)
{
    VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout DescriptorSetLayout;
    Device->CreateDescriptorSetLayout(
        Device->Handle,
        &Info,
        NULL,
        &DescriptorSetLayout);

    return DescriptorSetLayout;
}

void Rr_InvalidateDescriptorState(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *PipelineLayout)
{
    bool Disturbed = false;
    for(size_t Index = 0; Index < PipelineLayout->SetLayoutCount; ++Index)
    {
        Rr_DescriptorSetState *SetState = &State->SetStates[Index];
        VkDescriptorSetLayout Current = SetState->Layout;
        VkDescriptorSetLayout New = PipelineLayout->SetLayouts[Index]->Handle;
        if(Current != New)
        {
            Disturbed = true;
        }
        if(Disturbed)
        {
            SetState->Layout = New;
            SetState->Flags = RR_DESCRIPTOR_SET_STATE_FLAG_DIRTY_BIT;
            memset(
                SetState->Bindings,
                0,
                sizeof(Rr_DescriptorSetBinding) * RR_MAX_BINDINGS);
        }
    }
    State->Dirty = true;
}

void Rr_UpdateDescriptorsState(
    Rr_DescriptorsState *State,
    size_t SetIndex,
    size_t BindingIndex,
    Rr_DescriptorSetBinding *Binding)
{
    memcpy(
        &State->SetStates[SetIndex].Bindings[BindingIndex],
        Binding,
        sizeof(Rr_DescriptorSetBinding));
    State->SetStates[SetIndex].Flags |= RR_DESCRIPTOR_SET_STATE_FLAG_DIRTY_BIT;
    State->SetStates[SetIndex].Flags |= (1 << BindingIndex);
    State->Dirty = true;
}

static void Rr_ValidateNullSetBinding(
    Rr_PipelineBindingSet *Set,
    size_t Binding)
{
    for(size_t Index = 0; Index < Set->BindingCount; ++Index)
    {
        Rr_PipelineBinding *PipelineBinding = &Set->Bindings[Index];
        if(PipelineBinding->Binding == Binding && PipelineBinding->Count != 0)
        {
            RR_ABORT("Missing layout binding %zu!", Binding);
        }
    }
}

void Rr_ApplyDescriptorsState(
    Rr_DescriptorsState *State,
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_PipelineLayout *PipelineLayout,
    Rr_Device *Device,
    VkCommandBuffer CommandBuffer,
    VkPipelineBindPoint PipelineBindPoint)
{
    if(State->Dirty == false)
    {
        return;
    }

    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    Rr_DescriptorWriter *Writer =
        Rr_CreateDescriptorWriter(0, 0, 0, Scratch.Arena);

    bool FirstSetSet = false;
    bool Disturbed = false;
    uint32_t FirstSet = 0;
    uint32_t DescriptorSetCount = 0;
    VkDescriptorSet DescriptorSets[RR_MAX_SETS];
    uint32_t DynamicOffsetCount = 0;
    uint32_t DynamicOffsets[RR_MAX_BINDINGS * RR_MAX_SETS];

    for(uint32_t SetIndex = 0; SetIndex < RR_MAX_SETS; ++SetIndex)
    {
        Rr_DescriptorSetState *SetState = State->SetStates + SetIndex;

        bool Dirty = RR_HAS_BIT(
                         SetState->Flags,
                         RR_DESCRIPTOR_SET_STATE_FLAG_DIRTY_BIT) == true;

        if(Disturbed || Dirty)
        {
            Disturbed = true;
            if(FirstSetSet == false)
            {
                FirstSet = SetIndex;
                FirstSetSet = true;
            }
        }
        else
        {
            continue;
        }

        for(uint32_t BindingIndex = 0; BindingIndex < RR_MAX_BINDINGS;
            ++BindingIndex)
        {
            Rr_DescriptorSetBinding *Binding =
                SetState->Bindings + BindingIndex;

            if(RR_HAS_BIT(SetState->Flags, (1 << BindingIndex)) != true)
            {
#if defined(RR_DEBUG)
                Rr_ValidateNullSetBinding(
                    &PipelineLayout->SetLayouts[SetIndex]->Set,
                    BindingIndex);
#endif
                continue;
            }

            switch(Binding->Type)
            {
                case RR_PIPELINE_BINDING_TYPE_SAMPLER:
                {
                    Rr_WriteSamplerDescriptor(
                        Writer,
                        BindingIndex,
                        0,
                        Binding->Sampler);
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_SAMPLED_IMAGE:
                {
                    Rr_WriteImageDescriptor(
                        Writer,
                        BindingIndex,
                        0,
                        Binding->Image.View,
                        Binding->Image.Layout,
                        VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE);
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_COMBINED_IMAGE_SAMPLER:
                {
                    Rr_WriteCombinedImageSamplerDescriptor(
                        Writer,
                        BindingIndex,
                        0,
                        Binding->Image.View,
                        Binding->Image.Sampler,
                        Binding->Image.Layout);
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_UNIFORM_BUFFER:
                {
                    Rr_WriteBufferDescriptor(
                        Writer,
                        BindingIndex,
                        Binding->Buffer.Handle,
                        Binding->Buffer.Size,
                        0, /* We rely on dynamic offsets! */
                        Rr_ToVulkanDescriptorType(Binding->Type),
                        Scratch.Arena);
                    DynamicOffsets[DynamicOffsetCount] = Binding->Buffer.Offset;
                    DynamicOffsetCount++;
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_STORAGE_BUFFER:
                {
                    Rr_WriteBufferDescriptor(
                        Writer,
                        BindingIndex,
                        Binding->Buffer.Handle,
                        Binding->Buffer.Size,
                        0, /* We rely on dynamic offsets! */
                        Rr_ToVulkanDescriptorType(Binding->Type),
                        Scratch.Arena);
                    DynamicOffsets[DynamicOffsetCount] = Binding->Buffer.Offset;
                    DynamicOffsetCount++;
                }
                break;
                case RR_PIPELINE_BINDING_TYPE_STORAGE_IMAGE:
                {
                    Rr_WriteImageDescriptor(
                        Writer,
                        BindingIndex,
                        0,
                        Binding->Image.View,
                        Binding->Image.Layout,
                        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
                }
                break;
                default:
                {
                    RR_ABORT("Not implemented!");
                }
                break;
            }
        }

        VkDescriptorSet DescriptorSet = Rr_AllocateDescriptorSet(
            DescriptorAllocator,
            Device,
            PipelineLayout->SetLayouts[SetIndex]->Handle);

        Rr_UpdateDescriptorSet(Writer, Device, DescriptorSet);
        Rr_ResetDescriptorWriter(Writer);

        DescriptorSets[DescriptorSetCount++] = DescriptorSet;

        bool LastSet = SetIndex == PipelineLayout->SetLayoutCount - 1;
        if(LastSet && DescriptorSetCount > 0)
        {
            Device->CmdBindDescriptorSets(
                CommandBuffer,
                PipelineBindPoint,
                PipelineLayout->Handle,
                FirstSet,
                DescriptorSetCount,
                DescriptorSets,
                DynamicOffsetCount,
                DynamicOffsets);
            break;
        }
    }

    State->Dirty = false;

    Rr_DestroyScratch(Scratch);
}
