#include "Rr_Descriptor.h"

#include "Rr_Defines.h"
#include "Rr_Memory.h"

#include <SDL3/SDL_stdinc.h>

static VkDescriptorPool CreateDescriptorPool(
    VkDevice Device,
    usize SetCount,
    Rr_DescriptorPoolSizeRatio* Ratios,
    usize RatioCount)
{
    VkDescriptorPoolSize* PoolSizes = Rr_StackAlloc(VkDescriptorPoolSize, RatioCount);
    for (usize Index = 0; Index < RatioCount; Index++)
    {
        const Rr_DescriptorPoolSizeRatio* Ratio = &Ratios[Index];
        PoolSizes[Index] = (VkDescriptorPoolSize){ .type = Ratio->Type, .descriptorCount = (u32)(Ratio->Ratio * (f32)SetCount) };
    }

    const VkDescriptorPoolCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = 0,
        .maxSets = SetCount,
        .poolSizeCount = RatioCount,
        .pPoolSizes = PoolSizes
    };

    VkDescriptorPool NewPool;
    vkCreateDescriptorPool(Device, &Info, NULL, &NewPool);
    return NewPool;
}

Rr_DescriptorAllocator Rr_CreateDescriptorAllocator(
    VkDevice Device,
    usize MaxSets,
    Rr_DescriptorPoolSizeRatio* Ratios,
    usize RatioCount,
    Rr_Arena* Arena)
{
    Rr_DescriptorAllocator DescriptorAllocator = { 0 };
    DescriptorAllocator.Arena = Arena;
    Rr_SliceReserve(&DescriptorAllocator.Ratios, RatioCount, Arena);
    SDL_memcpy(DescriptorAllocator.Ratios.Data, Ratios, RatioCount * sizeof(Rr_DescriptorPoolSizeRatio));

    VkDescriptorPool NewPool = CreateDescriptorPool(Device, MaxSets, Ratios, RatioCount);
    Rr_SliceReserve(&DescriptorAllocator.ReadyPools, 1, Arena);
    *Rr_SlicePush(&DescriptorAllocator.ReadyPools, Arena) = NewPool;

    Rr_SliceReserve(&DescriptorAllocator.FullPools, 1, Arena);

    DescriptorAllocator.SetsPerPool = (usize)((f32)MaxSets * 1.5f);

    return DescriptorAllocator;
}

void Rr_ResetDescriptorAllocator(
    Rr_DescriptorAllocator* DescriptorAllocator,
    VkDevice Device)
{
    usize Count = Rr_SliceLength(&DescriptorAllocator->ReadyPools);
    for (usize Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools.Data[Index];
        vkResetDescriptorPool(Device, ReadyPool, 0);
    }

    Count = Rr_SliceLength(&DescriptorAllocator->FullPools);
    for (usize Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        vkResetDescriptorPool(Device, FullPool, 0);

        *Rr_SlicePush(&DescriptorAllocator->ReadyPools, NULL) = FullPool;
    }
    Rr_SliceEmpty(&DescriptorAllocator->FullPools);
}

void Rr_DestroyDescriptorAllocator(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    usize Count = Rr_SliceLength(&DescriptorAllocator->ReadyPools);
    for (usize Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool ReadyPool = DescriptorAllocator->ReadyPools.Data[Index];
        vkDestroyDescriptorPool(Device, ReadyPool, NULL);
    }
    Rr_SliceEmpty(&DescriptorAllocator->ReadyPools);

    Count = Rr_SliceLength(&DescriptorAllocator->FullPools);
    for (usize Index = 0; Index < Count; Index++)
    {
        VkDescriptorPool FullPool = DescriptorAllocator->FullPools.Data[Index];
        vkDestroyDescriptorPool(Device, FullPool, NULL);
    }
}

VkDescriptorPool Rr_GetDescriptorPool(Rr_DescriptorAllocator* DescriptorAllocator, VkDevice Device)
{
    VkDescriptorPool NewPool;
    const usize ReadyCount = Rr_SliceLength(&DescriptorAllocator->ReadyPools);
    if (ReadyCount != 0)
    {
        NewPool = DescriptorAllocator->ReadyPools.Data[ReadyCount - 1];
        Rr_SlicePop(&DescriptorAllocator->ReadyPools);
    }
    else
    {
        NewPool = CreateDescriptorPool(
            Device,
            DescriptorAllocator->SetsPerPool,
            DescriptorAllocator->Ratios.Data,
            Rr_SliceLength(&DescriptorAllocator->Ratios));

        DescriptorAllocator->SetsPerPool = (usize)((float)DescriptorAllocator->SetsPerPool * 1.5f);

        if (DescriptorAllocator->SetsPerPool > 4096)
        {
            DescriptorAllocator->SetsPerPool = 4096;
        }
    }

    return NewPool;
}

VkDescriptorSet Rr_AllocateDescriptorSet(
    Rr_DescriptorAllocator* DescriptorAllocator,
    VkDevice Device,
    VkDescriptorSetLayout Layout)
{
    VkDescriptorPool Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);

    VkDescriptorSetAllocateInfo AllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = Pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &Layout
    };

    VkDescriptorSet DescriptorSet;
    const VkResult Result = vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);

    if (Result == VK_ERROR_OUT_OF_POOL_MEMORY || Result == VK_ERROR_FRAGMENTED_POOL)
    {
        *Rr_SlicePush(&DescriptorAllocator->FullPools, DescriptorAllocator->Arena) = Pool;

        Pool = Rr_GetDescriptorPool(DescriptorAllocator, Device);
        AllocateInfo.descriptorPool = Pool;

        vkAllocateDescriptorSets(Device, &AllocateInfo, &DescriptorSet);
    }

    *Rr_SlicePush(&DescriptorAllocator->ReadyPools, DescriptorAllocator->Arena) = Pool;
    return DescriptorSet;
}

Rr_DescriptorWriter Rr_CreateDescriptorWriter(usize Images, usize Buffers, Rr_Arena* Arena)
{
    Rr_DescriptorWriter Writer = { 0 };
    Rr_SliceReserve(&Writer.ImageInfos, Images, Arena);
    Rr_SliceReserve(&Writer.BufferInfos, Buffers, Arena);
    Rr_SliceReserve(&Writer.Writes, Images + Buffers, Arena);
    Rr_SliceReserve(&Writer.Entries, Images + Buffers, Arena);
    return Writer;
}

void Rr_WriteImageDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    Rr_Arena* Arena)
{
    Rr_WriteImageDescriptorAt(Writer, Binding, 0, View, Sampler, Layout, Type, Arena);
}

void Rr_WriteImageDescriptorAt(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    u32 Index,
    VkImageView View,
    VkSampler Sampler,
    VkImageLayout Layout,
    VkDescriptorType Type,
    Rr_Arena* Arena)
{
    *Rr_SlicePush(&Writer->ImageInfos, Arena) = ((VkDescriptorImageInfo){ .sampler = Sampler, .imageView = View, .imageLayout = Layout });

    *Rr_SlicePush(&Writer->Writes, Arena) = ((VkWriteDescriptorSet){ .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstBinding = Binding, .dstSet = VK_NULL_HANDLE, .descriptorCount = 1, .descriptorType = Type, .dstArrayElement = Index });

    *Rr_SlicePush(&Writer->Entries, Arena) = ((Rr_DescriptorWriterEntry){ .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_IMAGE, .Index = Rr_SliceLength(&Writer->ImageInfos) - 1 });
}

void Rr_WriteBufferDescriptor(
    Rr_DescriptorWriter* Writer,
    u32 Binding,
    VkBuffer Buffer,
    usize Size,
    usize Offset,
    VkDescriptorType Type,
    Rr_Arena* Arena)
{
    *Rr_SlicePush(&Writer->BufferInfos, Arena) = ((VkDescriptorBufferInfo){ .range = Size, .buffer = Buffer, .offset = Offset });

    *Rr_SlicePush(&Writer->Writes, Arena) = ((VkWriteDescriptorSet){
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstBinding = Binding,
        .dstSet = VK_NULL_HANDLE,
        .descriptorCount = 1,
        .descriptorType = Type,
    });

    *Rr_SlicePush(&Writer->Entries, Arena) = ((Rr_DescriptorWriterEntry){ .Type = RR_DESCRIPTOR_WRITER_ENTRY_TYPE_BUFFER, .Index = Rr_SliceLength(&Writer->BufferInfos) - 1 });
}

void Rr_ResetDescriptorWriter(Rr_DescriptorWriter* Writer)
{
    Rr_SliceEmpty(&Writer->ImageInfos);
    Rr_SliceEmpty(&Writer->BufferInfos);
    Rr_SliceEmpty(&Writer->Writes);
    Rr_SliceEmpty(&Writer->Entries);
}

void Rr_UpdateDescriptorSet(Rr_DescriptorWriter* Writer, VkDevice Device, VkDescriptorSet Set)
{
    const usize WritesCount = Rr_SliceLength(&Writer->Writes);
    for (usize Index = 0; Index < WritesCount; ++Index)
    {
        const Rr_DescriptorWriterEntry* Entry = &Writer->Entries.Data[Index];
        VkWriteDescriptorSet* Write = &Writer->Writes.Data[Index];
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

    vkUpdateDescriptorSets(Device, (u32)WritesCount, Writer->Writes.Data, 0, NULL);
}

void Rr_AddDescriptor(Rr_DescriptorLayoutBuilder* Builder, u32 Binding, VkDescriptorType Type)
{
    if (Builder->Count >= RR_MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Type,
        .descriptorCount = 1,
    };
    Builder->Count++;
}

void Rr_AddDescriptorArray(
    Rr_DescriptorLayoutBuilder* Builder,
    u32 Binding,
    u32 Count,
    VkDescriptorType Type)
{
    if (Builder->Count >= RR_MAX_LAYOUT_BINDINGS)
    {
        return;
    }
    Builder->Bindings[Builder->Count] = (VkDescriptorSetLayoutBinding){
        .binding = Binding,
        .descriptorType = Type,
        .descriptorCount = Count,
    };
    Builder->Count++;
}

void Rr_ClearDescriptors(Rr_DescriptorLayoutBuilder* Builder)
{
    *Builder = (Rr_DescriptorLayoutBuilder){ 0 };
}

VkDescriptorSetLayout Rr_BuildDescriptorLayout(
    Rr_DescriptorLayoutBuilder* Builder,
    VkDevice Device,
    VkShaderStageFlags ShaderStageFlags)
{
    VkDescriptorBindingFlags BindingFlags[Builder->Count];
    for (u32 Index = 0; Index < Builder->Count; ++Index)
    {
        VkDescriptorSetLayoutBinding* Binding = &Builder->Bindings[Index];
        Binding->stageFlags |= ShaderStageFlags;

//        if (Binding->descriptorCount > 1)
//        {
//            BindingFlags[Index] = VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
//        }
//        else
        {
            BindingFlags[Index] = 0;
        }
    }

    VkDescriptorSetLayoutBindingFlagsCreateInfo BindingFlagsInfo = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
        .bindingCount = Builder->Count,
        .pBindingFlags = BindingFlags
    };

    const VkDescriptorSetLayoutCreateInfo Info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .pNext = &BindingFlagsInfo,
        .flags = 0,
        .bindingCount = Builder->Count,
        .pBindings = Builder->Bindings,
    };

    VkDescriptorSetLayout DescriptorSetLayout;
    vkCreateDescriptorSetLayout(Device, &Info, NULL, &DescriptorSetLayout);

    return DescriptorSetLayout;
}
