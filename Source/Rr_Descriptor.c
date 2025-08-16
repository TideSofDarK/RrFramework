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
    for (size_t Index = 0; Index < RatioCount; Index++)
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

Rr_DescriptorAllocator *Rr_CreateDescriptorAllocator(
    Rr_Device *Device,
    uint32_t MaxSets,
    Rr_DescriptorPoolSizeRatio *Ratios,
    uint32_t RatioCount)
{
    Rr_Arena *Arena = Rr_CreateDefaultArena();

    Rr_DescriptorAllocator *DescriptorAllocator =
        RR_ALLOC_TYPE(Arena, Rr_DescriptorAllocator);
    DescriptorAllocator->Arena = Arena;

    RR_RESERVE_ARRAY(&DescriptorAllocator->Ratios, RatioCount, Arena);
    memcpy(
        DescriptorAllocator->Ratios.Data,
        Ratios,
        RatioCount * sizeof(Rr_DescriptorPoolSizeRatio));
    DescriptorAllocator->Ratios.Count = RatioCount;

    VkDescriptorPool NewPool =
        Rr_CreateDescriptorPool(Device, MaxSets, Ratios, RatioCount);
    *RR_PUSH_INTO_ARRAY(&DescriptorAllocator->ReadyPools, Arena) = NewPool;

    RR_RESERVE_ARRAY(&DescriptorAllocator->FullPools, 1, Arena);

    DescriptorAllocator->SetsPerPool = (size_t)((float)MaxSets * 1.5f);

    return DescriptorAllocator;
}

void Rr_ResetDescriptorAllocator(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device)
{
    size_t Count = DescriptorAllocator->ReadyPools.Count;
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool =
            DescriptorAllocator->ReadyPools.Data[Index];
        Device->ResetDescriptorPool(Device->Handle, ReadyPool, 0);
    }

    Count = DescriptorAllocator->FullPools.Count;
    for (size_t Index = 0; Index < Count; Index++)
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
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool =
            DescriptorAllocator->ReadyPools.Data[Index];
        Device->DestroyDescriptorPool(Device->Handle, ReadyPool, NULL);
    }
    RR_CLEAR_ARRAY(&DescriptorAllocator->ReadyPools);

    Count = DescriptorAllocator->FullPools.Count;
    for (size_t Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        Device->DestroyDescriptorPool(Device->Handle, FullPool, NULL);
    }

    Rr_DestroyArena(DescriptorAllocator->Arena);
}

VkDescriptorPool Rr_GetDescriptorPool(
    Rr_DescriptorAllocator *DescriptorAllocator,
    Rr_Device *Device)
{
    VkDescriptorPool NewPool;
    size_t ReadyCount = DescriptorAllocator->ReadyPools.Count;
    if (ReadyCount != 0)
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

        if (DescriptorAllocator->SetsPerPool > 4096)
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

    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY ||
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
    if (WritesCount == 0)
    {
        return;
    }
    for (size_t Index = 0; Index < WritesCount; ++Index)
    {
        Rr_DescriptorWriterEntry *Entry = &Writer->Entries.Data[Index];
        VkWriteDescriptorSet *Write = &Writer->Writes.Data[Index];
        Write->dstSet = Set;
        switch (Entry->Type)
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
    if (Builder->Count >= RR_MAX_SETS)
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
    if (Builder->Count >= RR_MAX_SETS)
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

void Rr_InvalidateDescriptorsStateV2(
    Rr_DescriptorsState *State,
    Rr_PipelineLayout *Layout)
{
    if (State->Layout != NULL)
    {
        for (size_t Index = 0; Index < RR_MAX_SETS; ++Index)
        {
            VkDescriptorSetLayout OldLayout =
                State->Layout->SetLayouts[Index]->Handle;
            VkDescriptorSetLayout NewLayout = Layout->SetLayouts[Index]->Handle;
            if (OldLayout == NewLayout)
            {
                continue;
            }
            for (; Index < RR_MAX_SETS; ++Index)
            {
                State->Sets[Index] = VK_NULL_HANDLE;
            }
            break;
        }
    }

    State->Layout = Layout;
}

static inline void Rr_CopyDescriptorSet(
    VkDescriptorSet Dst,
    VkDescriptorSet Src,
    Rr_Device *Device,
    Rr_DescriptorSetLayout *Layout)
{
    Rr_Scratch Scratch = Rr_GetScratch(NULL);

    VkCopyDescriptorSet *Copies = RR_ALLOC_NO_ZERO(
        Scratch.Arena,
        Layout->Set.BindingCount * sizeof(VkCopyDescriptorSet));

    for (uint32_t Index = 0; Index < Layout->Set.BindingCount; ++Index)
    {
        Rr_PipelineBinding *Binding = &Layout->Set.Bindings[Index];

        Copies[Index] = (VkCopyDescriptorSet){
            .sType = VK_STRUCTURE_TYPE_COPY_DESCRIPTOR_SET,
            .srcSet = Src,
            .srcBinding = Binding->Index,
            .srcArrayElement = 0,
            .dstSet = Dst,
            .dstBinding = Binding->Index,
            .dstArrayElement = 0,
            .descriptorCount = Binding->Count,
        };
    }

    Device->UpdateDescriptorSets(
        Device->Handle,
        0,
        NULL,
        Layout->Set.BindingCount,
        Copies);

    Rr_DestroyScratch(Scratch);
}

static inline VkDescriptorSet Rr_GetDescriptorSet(
    Rr_DescriptorsState *State,
    uint32_t SetIndex)
{
    if (State->Sets[SetIndex] == VK_NULL_HANDLE || !State->Dirty[SetIndex])
    {
        VkDescriptorSet OldSet = State->Sets[SetIndex];

        State->Sets[SetIndex] = Rr_AllocateDescriptorSet(
            State->Allocator,
            State->Device,
            State->Layout->SetLayouts[SetIndex]->Handle);

        if (OldSet)
        {
            Rr_Device *Device = State->Device;
            Rr_CopyDescriptorSet(
                State->Sets[SetIndex],
                OldSet,
                State->Device,
                State->Layout->SetLayouts[SetIndex]);
        }

        State->Dirty[SetIndex] = true;
    }

    return State->Sets[SetIndex];
}

void Rr_WriteImageDescriptorV2(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkImageView View,
    VkImageLayout Layout,
    VkSampler Sampler)
{
    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
        .imageView = View,
        .imageLayout = Layout,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteBufferDescriptorV2(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkDescriptorType Type,
    VkBuffer Handle,
    uint32_t Size,
    uint32_t Offset)
{
    Rr_Device *Device = State->Device;

    VkDescriptorBufferInfo BufferInfo = {
        .buffer = Handle,
        .offset = Offset,
        .range = Size,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = Type,
        .pBufferInfo = &BufferInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_WriteSamplerDescriptorV2(
    Rr_DescriptorsState *State,
    uint32_t Set,
    uint32_t Binding,
    uint32_t ArrayIndex,
    VkSampler Sampler)
{
    Rr_Device *Device = State->Device;

    VkDescriptorImageInfo ImageInfo = {
        .sampler = Sampler,
    };

    VkWriteDescriptorSet Write = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = Rr_GetDescriptorSet(State, Set),
        .dstBinding = Binding,
        .dstArrayElement = ArrayIndex,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_SAMPLER,
        .pImageInfo = &ImageInfo,
    };

    Device->UpdateDescriptorSets(Device->Handle, 1, &Write, 0, NULL);
}

void Rr_ApplyDescriptorsStateV2(
    Rr_DescriptorsState *State,
    VkPipelineBindPoint BindPoint)
{
    Rr_Device *Device = State->Device;

    for (size_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        if (State->Layout->SetLayouts[Index])
        {
            if (State->Dirty[Index])
            {
                uint32_t Count = State->Layout->SetLayoutCount - Index;

                Device->CmdBindDescriptorSets(
                    State->CommandBuffer,
                    BindPoint,
                    State->Layout->Handle,
                    Index,
                    Count,
                    &State->Sets[Index],
                    0,
                    NULL);

                break;
            }
        }
    }

    for (size_t Index = 0; Index < RR_MAX_SETS; ++Index)
    {
        State->Dirty[Index] = false;
    }
}
